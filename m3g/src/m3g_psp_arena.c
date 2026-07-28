/*
 * m3g_psp_arena.c -- the private heap m3gcore allocates from.
 *
 * WHY THIS EXISTS
 * ===============
 * On PSPKVM the Java object heap is not a separate region of memory: it is one
 * enormous block taken out of the ordinary C heap.
 * javacall/implementation/psp_mips/common/memory.c:48
 * (javacall_memory_heap_allocate) reserves 512 KB, then loops
 *
 *     sz = 64 MB;  while (sz > size) { if ((p = malloc(sz))) break; sz -= 200K; }
 *
 * and hands the winner to the VM, which carves the pcsl pool and the object
 * heap out of it.  So after startup the C heap holds one ~30 MB live block
 * containing every Java object in the system, and roughly 512 KB of free space
 * around it.
 *
 * Before JSR-184 nothing much competed for those 512 KB.  m3gcore changes that
 * completely: it allocates its entire scene graph -- every Node, VertexBuffer,
 * IndexBuffer and animation track of every loaded .m3g -- through the
 * mallocFunc/freeFunc callbacks in M3Gparams, i.e. from that same C heap, a few
 * hundred bytes at a time, immediately adjacent to the Java heap block.  Two
 * bad things follow:
 *
 *   1. Adjacency.  Any out-of-bounds write by the engine, or any wrong-sized
 *      allocation on our side, lands in the neighbouring Java heap and silently
 *      rewrites VM metadata.  That is exactly the failure being chased here: a
 *      romized Method whose flags word read back as 0, so constants() returned
 *      NULL and anewarray faulted.
 *   2. Exhaustion.  Deep 3D loads 75 scenes and nothing ever releases them
 *      (the Java wrappers hold the engine references and CLDC has no
 *      finalization), so the engine's live set only grows.  Measured on the
 *      host harness the interface alone is 18.6 KB and a scene costs about six
 *      times its file size; the game's 131 KB of .m3g therefore needs on the
 *      order of 800 KB -- more than the whole free C heap.  Once malloc starts
 *      failing it fails for *everything*, including javacall and pcsl, whose
 *      allocation-failure paths are not all robust.
 *
 * WHAT THIS DOES
 * ==============
 * m3gcore gets its own heap, a plain static array.  A static array lives in
 * .bss, which the PSP module loader maps before the C heap is created, so it is
 * physically outside both the C heap and the Java heap.  The engine can no
 * longer reach VM memory by overrunning a block, and it can no longer starve
 * the rest of the runtime.
 *
 * The allocator is a boundary-tag first-fit heap with immediate coalescing --
 * ordinary, but every block carries a header magic and a trailing canary, the
 * arena carries a 64-byte guard band at each end, and both are verified on
 * every free and by m3gPspArenaVerify().  So the arena is not just a wall, it
 * is a detector: if the engine really does write out of bounds, the canary that
 * catches it names the block and the direction instead of leaving a corrupted
 * VM to fail somewhere unrelated ten seconds later.
 *
 * .bss is not stored in the executable, so this costs file size nothing; it
 * costs exactly M3G_PSP_ARENA_KB of RAM, which comes out of what the VM's
 * greedy loop would otherwise have taken for the Java heap.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include "M3G/m3g_psp.h"

/*----------------------------------------------------------------------
 * Tunables
 *--------------------------------------------------------------------*/

/*
 * Sized from measurement, not guesswork.  The host harness
 * (test/m3g_load_test.c) instrumented at the mallocFunc reports, for the two
 * unobfuscated scenes in the Deep 3D JAR, an 18.6 KB permanent interface plus
 * about 6 bytes of live scene graph per byte of file.  All 75 files together
 * are 131 KB, so a worst case of everything resident at once is ~800 KB.
 * 1536 KB leaves ~90% headroom for allocator overhead (24 bytes per block),
 * fragmentation, and the textures and vertex buffers the sceGu backend will
 * add later.
 *
 * The high-water mark is reported through m3gPspArenaGetStats() and logged by
 * the KNI layer after every load, so this number can be tuned against real
 * hardware behaviour rather than left to guesswork.
 */
#ifndef M3G_PSP_ARENA_KB
#define M3G_PSP_ARENA_KB 1536
#endif

#define ARENA_BYTES ((M3Guint) (M3G_PSP_ARENA_KB) * 1024u)

/*----------------------------------------------------------------------
 * Layout
 *
 *   [ guard 64 B ][ block ][ block ] ... [ block ][ guard 64 B ]
 *
 *   block := [ Header 16 B ][ payload, multiple of 8 ][ footer 8 B ]
 *--------------------------------------------------------------------*/

#define ALIGNMENT      8u
#define GUARD_BYTES    64u
#define GUARD_WORD     0x5A3CC3A5u

#define HDR_MAGIC      0x4D336841u   /* "M3hA" */
#define FTR_MAGIC      0x4D336646u   /* "M3fF" */

typedef struct Header {
    M3Guint magic;      /* HDR_MAGIC while the block is intact           */
    M3Guint size;       /* payload bytes, always a multiple of ALIGNMENT */
    M3Guint prevStride; /* total size of the physically previous block   */
    M3Guint used;       /* 0 free, 1 in use                              */
} Header;

#define HDR_SIZE  ((M3Guint) sizeof(Header))     /* 16 */
#define FTR_SIZE  (2u * (M3Guint) sizeof(M3Guint)) /* 8: magic + size copy */
#define OVERHEAD  (HDR_SIZE + FTR_SIZE)          /* 24 */

/* The smallest payload worth splitting a block for. */
#define MIN_PAYLOAD ALIGNMENT

/*----------------------------------------------------------------------
 * State
 *--------------------------------------------------------------------*/

/*
 * Wrapped in a union with a double so the base address is 8-aligned by the
 * language rather than by luck.  A bare M3Gubyte or M3Guint array would only
 * promise 1 or 4, and every payload address in here is base + a multiple of 8,
 * so the whole heap would inherit the shortfall -- on MIPS an ldc1/sdc1 to a
 * 4-aligned address is an alignment exception, not a slow path.
 */
static union {
    double   forceAlign;
    M3Gubyte bytes[ARENA_BYTES];
} s_arenaStore;

#define s_arena (s_arenaStore.bytes)

static M3Gbool  s_ready      = M3G_FALSE;
static M3Gubyte *s_first     = 0;   /* first block header                */
static M3Gubyte *s_end       = 0;   /* one past the last block           */
static M3Gubyte *s_rover     = 0;   /* next-fit cursor                   */

static M3Guint  s_used       = 0;   /* payload bytes handed out          */
static M3Guint  s_peak       = 0;
static M3Guint  s_blocks     = 0;   /* live blocks                       */
static M3Guint  s_failures   = 0;   /* allocations that could not be met */
static M3Guint  s_corrupt    = 0;   /* canary violations observed        */
static M3Guint  s_firstBad   = 0;   /* address of the first bad block    */
static M3Gint   s_lastFault  = M3G_PSP_ARENA_OK;

/*----------------------------------------------------------------------
 * Block helpers
 *--------------------------------------------------------------------*/

static M3Guint alignUp(M3Guint n)
{
    return (n + (ALIGNMENT - 1u)) & ~(ALIGNMENT - 1u);
}

static M3Guint blockStride(const Header *h)
{
    return OVERHEAD + h->size;
}

static M3Guint *blockFooter(Header *h)
{
    return (M3Guint *) ((M3Gubyte *) h + HDR_SIZE + h->size);
}

static void writeFooter(Header *h)
{
    M3Guint *f = blockFooter(h);
    f[0] = FTR_MAGIC;
    f[1] = h->size;
}

static M3Gbool footerOk(Header *h)
{
    const M3Guint *f = blockFooter(h);
    return (M3Gbool) (f[0] == FTR_MAGIC && f[1] == h->size);
}

static Header *nextBlock(Header *h)
{
    M3Gubyte *n = (M3Gubyte *) h + blockStride(h);
    return (n < s_end) ? (Header *) n : 0;
}

static Header *prevBlock(Header *h)
{
    if (h->prevStride == 0) {
        return 0;
    }
    return (Header *) ((M3Gubyte *) h - h->prevStride);
}

/*!
 * \internal
 * \brief Records a canary violation.
 *
 * Sticky: the first one is the one that matters, later ones are usually
 * collateral from walking a heap that is already wrong.
 */
static void reportCorruption(M3Gint fault, const void *where)
{
    ++s_corrupt;
    if (s_firstBad == 0) {
        s_firstBad  = (M3Guint) where;
        s_lastFault = fault;
    }
}

/*----------------------------------------------------------------------
 * Setup
 *--------------------------------------------------------------------*/

static void arenaInit(void)
{
    M3Gubyte *base = (M3Gubyte *) s_arena;
    M3Guint i;
    Header *h;

    /* Guard bands.  Nothing in the arena is ever allowed to write here, so a
     * disturbed word is unambiguous proof of an overrun off either end. */
    for (i = 0; i < GUARD_BYTES / sizeof(M3Guint); ++i) {
        ((M3Guint *) base)[i] = GUARD_WORD;
        ((M3Guint *) (base + ARENA_BYTES - GUARD_BYTES))[i] = GUARD_WORD;
    }

    s_first = base + GUARD_BYTES;
    s_end   = base + ARENA_BYTES - GUARD_BYTES;

    h = (Header *) s_first;
    h->magic      = HDR_MAGIC;
    h->size       = (M3Guint) (s_end - s_first) - OVERHEAD;
    h->size      &= ~(ALIGNMENT - 1u);
    h->prevStride = 0;
    h->used       = 0;
    writeFooter(h);

    /* The rounding above can leave a few unusable bytes before s_end; pull
     * s_end in so the block chain ends exactly where the walk expects. */
    s_end  = (M3Gubyte *) h + blockStride(h);
    s_rover = s_first;
    s_ready = M3G_TRUE;
}

/*----------------------------------------------------------------------
 * Allocation
 *--------------------------------------------------------------------*/

/*!
 * \internal
 * \brief Splits \c h so that it serves exactly \c want payload bytes.
 *
 * Does nothing unless the remainder is big enough to be a block in its own
 * right, since a header with no usable payload is worse than the waste.
 */
static void splitBlock(Header *h, M3Guint want)
{
    M3Guint spare = h->size - want;
    Header *tail, *after;

    if (spare < OVERHEAD + MIN_PAYLOAD) {
        return;
    }

    h->size = want;
    writeFooter(h);

    tail = (Header *) ((M3Gubyte *) h + blockStride(h));
    tail->magic      = HDR_MAGIC;
    tail->size       = spare - OVERHEAD;
    tail->prevStride = blockStride(h);
    tail->used       = 0;
    writeFooter(tail);

    after = nextBlock(tail);
    if (after != 0) {
        after->prevStride = blockStride(tail);
    }
}

void *m3gPspArenaAlloc(M3Guint bytes)
{
    M3Guint want;
    M3Gubyte *start;
    Header *h;
    M3Gbool wrapped = M3G_FALSE;

    if (!s_ready) {
        arenaInit();
    }

    /* m3gcore does ask for zero-length blocks (empty vertex arrays); newlib
     * malloc may answer NULL for those, which the engine would read as out of
     * memory.  Give them the minimum block instead. */
    want = alignUp(bytes);
    if (want == 0) {
        want = MIN_PAYLOAD;
    }
    if (want > (M3Guint) (s_end - s_first)) {
        ++s_failures;
        return 0;
    }

    /* Next fit: start where the last allocation finished and wrap once. */
    start = s_rover;
    h = (Header *) start;

    for (;;) {
        if (h->magic != HDR_MAGIC || !footerOk(h)) {
            reportCorruption(h->magic != HDR_MAGIC
                                 ? M3G_PSP_ARENA_BAD_HEADER
                                 : M3G_PSP_ARENA_BAD_FOOTER,
                             h);
            ++s_failures;
            return 0;   /* the free list is not trustworthy any more */
        }

        if (!h->used && h->size >= want) {
            splitBlock(h, want);
            h->used = 1;
            s_used += h->size;
            ++s_blocks;
            if (s_used > s_peak) {
                s_peak = s_used;
            }
            s_rover = (M3Gubyte *) h + blockStride(h);
            if (s_rover >= s_end) {
                s_rover = s_first;
            }
            return (M3Gubyte *) h + HDR_SIZE;
        }

        h = nextBlock(h);
        if (h == 0) {
            if (wrapped) {
                break;
            }
            wrapped = M3G_TRUE;
            h = (Header *) s_first;
        }
        if (wrapped && (M3Gubyte *) h >= start) {
            break;
        }
    }

    ++s_failures;
    return 0;
}

/*----------------------------------------------------------------------
 * Release
 *--------------------------------------------------------------------*/

/*!
 * \internal
 * \brief Merges \c h with the following block if that one is free.
 */
static void coalesceForward(Header *h)
{
    Header *n = nextBlock(h);
    Header *after;

    if (n == 0 || n->magic != HDR_MAGIC || n->used) {
        return;
    }
    h->size += blockStride(n);
    writeFooter(h);

    after = nextBlock(h);
    if (after != 0) {
        after->prevStride = blockStride(h);
    }
}

void m3gPspArenaFree(void *ptr)
{
    Header *h;

    if (ptr == 0) {
        return;
    }
    if (!s_ready
        || (M3Gubyte *) ptr < s_first + HDR_SIZE
        || (M3Gubyte *) ptr >= s_end) {
        /* Not ours.  Cannot happen with the current wiring -- every engine
         * allocation goes through m3gPspArenaAlloc -- so say so rather than
         * corrupting the heap by treating it as a block. */
        reportCorruption(M3G_PSP_ARENA_FOREIGN_PTR, ptr);
        return;
    }

    h = (Header *) ((M3Gubyte *) ptr - HDR_SIZE);

    if (h->magic != HDR_MAGIC) {
        /* Something wrote across the front of this block -- i.e. the block
         * before it overran, or the caller passed a bogus pointer. */
        reportCorruption(M3G_PSP_ARENA_BAD_HEADER, h);
        return;
    }
    if (!footerOk(h)) {
        /* This block overran its own payload: the canary immediately after
         * the payload is gone.  This is the one that names the culprit. */
        reportCorruption(M3G_PSP_ARENA_BAD_FOOTER, h);
        return;
    }
    if (!h->used) {
        reportCorruption(M3G_PSP_ARENA_DOUBLE_FREE, h);
        return;
    }

    h->used = 0;
    s_used -= h->size;
    --s_blocks;

    coalesceForward(h);
    {
        Header *p = prevBlock(h);
        if (p != 0 && p->magic == HDR_MAGIC && !p->used) {
            coalesceForward(p);
            h = p;
        }
    }

    /* The rover may have been pointing into the block that just disappeared. */
    s_rover = (M3Gubyte *) h;
}

/*----------------------------------------------------------------------
 * Verification and reporting
 *--------------------------------------------------------------------*/

M3Gint m3gPspArenaVerify(void)
{
    M3Gubyte *base = (M3Gubyte *) s_arena;
    M3Guint i, prev = 0;
    Header *h;

    if (!s_ready) {
        return M3G_PSP_ARENA_OK;
    }

    for (i = 0; i < GUARD_BYTES / sizeof(M3Guint); ++i) {
        if (((M3Guint *) base)[i] != GUARD_WORD) {
            reportCorruption(M3G_PSP_ARENA_HEAD_GUARD, base + i * sizeof(M3Guint));
            return M3G_PSP_ARENA_HEAD_GUARD;
        }
        if (((M3Guint *) (base + ARENA_BYTES - GUARD_BYTES))[i] != GUARD_WORD) {
            reportCorruption(M3G_PSP_ARENA_TAIL_GUARD,
                             base + ARENA_BYTES - GUARD_BYTES + i * sizeof(M3Guint));
            return M3G_PSP_ARENA_TAIL_GUARD;
        }
    }

    for (h = (Header *) s_first; h != 0; h = nextBlock(h)) {
        if (h->magic != HDR_MAGIC) {
            reportCorruption(M3G_PSP_ARENA_BAD_HEADER, h);
            return M3G_PSP_ARENA_BAD_HEADER;
        }
        if (!footerOk(h)) {
            reportCorruption(M3G_PSP_ARENA_BAD_FOOTER, h);
            return M3G_PSP_ARENA_BAD_FOOTER;
        }
        if (h->prevStride != prev) {
            reportCorruption(M3G_PSP_ARENA_BAD_CHAIN, h);
            return M3G_PSP_ARENA_BAD_CHAIN;
        }
        prev = blockStride(h);
    }

    return M3G_PSP_ARENA_OK;
}

void m3gPspArenaGetStats(M3GPspArenaStats *out)
{
    if (out == 0) {
        return;
    }
    out->capacity = (M3Gint) (ARENA_BYTES - 2u * GUARD_BYTES);
    out->used     = (M3Gint) s_used;
    out->peak     = (M3Gint) s_peak;
    out->blocks   = (M3Gint) s_blocks;
    out->failures = (M3Gint) s_failures;
    out->corrupt  = (M3Gint) s_corrupt;
    out->firstBad = (M3Gint) s_firstBad;
    out->fault    = s_lastFault;
}
