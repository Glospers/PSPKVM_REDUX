/*
 * m3g_psp_vidmem.c -- keeps pspgl out of the edram PSPKVM already owns.
 *
 * Two allocators want the PSP's 2 MB of embedded VRAM and neither knows the
 * other exists:
 *
 *   - PSPKVM's, psp/vram.c.  getStaticVramBuffer() hands out sequential
 *     offsets from edram + 0 and never frees.  psp/pspkvm.c:364-366 takes
 *     three buffers out of it before sceGuInit() -- two 512x272 8888 frame
 *     buffers and a 512x272 4444 depth buffer, 0x154000 bytes in total -- and
 *     the MIDP screen is blitted into them as a textured quad on every flush
 *     (javacall/implementation/psp_mips/midp/lcd.c:121-138).
 *
 *   - pspgl's, __pspgl_vidmem_alloc().  First fit from sceGeEdramGetAddr(),
 *     i.e. also from edram + 0, with eviction and compaction on top that
 *     relocate blocks using a map containing only pspgl's own allocations.
 *
 * Left alone the two overlap exactly: pspgl's first surface lands on PSPKVM's
 * front buffer.  The coexistence experiment in m3g/test/pspgl/coexist/ showed
 * both calling sceDisplaySetFrameBuf on the same two addresses, which looked
 * survivable only because the sizes and allocation order happened to coincide
 * and both cleared the whole screen every frame.
 *
 * The fix is to tell pspgl the truth once, before it allocates anything: a
 * single dummy block is pushed through pspgl's own allocator, so its map
 * starts above what PSPKVM has taken and every later allocation -- including
 * eviction and compaction, which work off that same map -- respects it.
 *
 * HOW MUCH IS RESERVED, AND WHY IT IS ALL OF IT
 *
 * The obvious size is exactly PSPKVM's high-water mark, 0x154000, leaving
 * pspgl the remaining 0xAC000 (704 KB).  That does not work, and the way it
 * fails is worth writing down because it is not obvious.
 *
 * The render target is a 480x272 pbuffer.  pspgl rounds the width up to a
 * power of two, so its colour buffer is 512*272*4 = 544 KB and its depth
 * buffer 512*272*2 = 272 KB: 816 KB together, more than 704 KB but with the
 * colour buffer alone fitting.  So pspgl places the colour buffer in edram,
 * then finds it has nowhere for the depth buffer, and does what it is supposed
 * to do -- evicts (pspgl_buffers.c:207).  Eviction copies the buffer out to
 * system memory *with the GE*, and the GE path dereferences the current
 * context (__pspgl_copy_pixels -> __pspgl_context_flush_pending_state_changes
 * (pspgl_curctx, ...)).  During m3gBindMemoryTarget no context is current yet
 * -- m3gcore only calls eglMakeCurrent afterwards, from m3gMakeGLCurrent -- so
 * pspgl_curctx is NULL and the read faults at 0x2d0.
 *
 * Reserving *everything* removes that failure mode at the root rather than
 * papering over it.  With no edram left, __pspgl_vidmem_alloc fails on the
 * first try, evict_vidmem walks pspgl's buffer list and finds nothing with an
 * edram address to move (pspgl_buffers.c:159), and __pspgl_vidmem_compact
 * skips the reservation because it is BF_PINNED_FIXED (pspgl_vidmem.c:267) --
 * so no GE command is ever emitted and __pspgl_buffer_init falls through to
 * memalign (pspgl_buffers.c:238).  Every pspgl buffer lives in main memory.
 * The GE renders there perfectly well; it is slower than edram, and that is
 * the price until the backend keeps a context current across binds, at which
 * point the reservation can shrink to the high-water mark and the colour
 * buffer can move back into edram.
 *
 * The block is never freed, never appears on pspgl's buffer LRU list (that
 * list is built by __pspgl_buffer_new, which is not used here), and so is
 * never a candidate for eviction.  It costs 32 bytes of .bss, which matters:
 * psp/pspkvm.c requests a fixed 32 MB heap after the module loads
 * (docker/patches/0031) and that request fails silently -- the runtime hangs on
 * the splash screen with no diagnostic -- if .bss has grown enough to make it
 * unsatisfiable.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include "M3G/m3g_psp.h"

#include <stddef.h>     /* offsetof, NULL */

/*----------------------------------------------------------------------
 * pspgl internals
 *
 * struct pspgl_buffer is private to pspgl (pspgl_buffers.h), so it is
 * mirrored here rather than included.  Only two of its fields are touched --
 * size on the way in, base on the way out -- but the whole layout has to
 * match because __pspgl_vidmem_alloc() stores the pointer in its map and
 * __pspgl_vidmem_free()/_evict()/_compact() read it back.  The static
 * assertion below fails the build if the layout ever stops matching the
 * pspgl in the toolchain.
 *--------------------------------------------------------------------*/

struct m3gPspGlBuffer {
    short           refcount;
    short           generation;
    signed char     mapped;
    unsigned char   flags;
    void           *pin_prevp;
    void           *pin_next;
    void           *list_prev;
    void           *list_next;
    void           *base;
    int             size;
};

/* pspgl_buffers.h: BF_PINNED_FIXED marks a block that must never be evicted,
 * BF_UNMANAGED one whose memory pspgl did not allocate.  Neither is consulted
 * for a block that is not on the buffer list, but both describe this block
 * accurately and cost nothing. */
#define M3G_BF_PINNED_FIXED  (1 << 2)
#define M3G_BF_UNMANAGED     (1 << 3)

typedef char m3gPspGlBufferLayoutCheck[
    (sizeof(struct m3gPspGlBuffer) == 32
     && offsetof(struct m3gPspGlBuffer, base) == 24
     && offsetof(struct m3gPspGlBuffer, size) == 28) ? 1 : -1];

extern int __pspgl_vidmem_alloc(struct m3gPspGlBuffer *buf);

/*----------------------------------------------------------------------
 * How much to reserve
 *
 * M3G_PSP_VRAM_RESERVE_KB overrides the default of "all of it"; the Makefile
 * sets it to 0, which this clamps up to PSPKVM's high-water mark -- i.e.
 * reserve exactly what PSPKVM holds and leave pspgl the rest of edram.
 *
 * The note at the top of this file explains why it was once "all of it": to
 * keep pspgl off the eviction path, which reaches the GE through a
 * current-context global that was NULL during target binding.  The port now
 * holds a context for the life of the process (m3gPspHoldGLContext), so
 * eviction has a context and the reservation no longer has to be total.  That
 * matters because a total reservation forced every pspgl buffer into memalign
 * from the C heap, alongside the Java object heap.
 *
 * sceGeEdramGetSize and getStaticVramUsed are both declared weak: the former
 * so the host-side test harness in m3g/test/ links without the PSP SDK, the
 * latter because it is added by docker/patches/0045 and the romgen host tool
 * never has it.  psp/vram.c keeps the running offset in a file static, which
 * is why the accessor is needed at all.
 *--------------------------------------------------------------------*/

extern unsigned int sceGeEdramGetSize(void) __attribute__((weak));
extern unsigned int getStaticVramUsed(void) __attribute__((weak));

/* The PSP's embedded VRAM, used when sceGeEdramGetSize is not linked in. */
#define PSP_EDRAM_BYTES (2 * 1024 * 1024)

/* psp/pspkvm.c:364-366: two 512x272 8888 buffers and one 512x272 4444. */
#define PSPKVM_BUF_WIDTH   512
#define PSPKVM_SCR_HEIGHT  272
#define PSPKVM_FALLBACK_VRAM_USED \
    (PSPKVM_BUF_WIDTH * PSPKVM_SCR_HEIGHT * 4 * 2 + \
     PSPKVM_BUF_WIDTH * PSPKVM_SCR_HEIGHT * 2)

/*----------------------------------------------------------------------
 * The reservation
 *--------------------------------------------------------------------*/

static struct m3gPspGlBuffer s_reservation;
static M3Gint s_state = 0;   /* 0 = not attempted, 1 = held, -1 = failed */

M3Gint m3gPspReserveVram(void)
{
    unsigned int used, total;
    int i;

    if (s_state != 0) {
        return s_state;
    }

    total = (sceGeEdramGetSize != 0)
          ? sceGeEdramGetSize()
          : (unsigned int) PSP_EDRAM_BYTES;

#if defined(M3G_PSP_VRAM_RESERVE_KB)
    used = (unsigned int) (M3G_PSP_VRAM_RESERVE_KB) * 1024u;
#else
    used = total;
#endif

    /* Whatever was asked for, never hand pspgl anything PSPKVM is already
     * displaying out of. */
    {
        unsigned int owned = (getStaticVramUsed != 0)
                           ? getStaticVramUsed()
                           : (unsigned int) PSPKVM_FALLBACK_VRAM_USED;
        if (used < owned) {
            used = owned;
        }
    }
    if (used > total) {
        used = total;
    }
    if (used == 0) {
        /* Nothing to protect -- the whole of edram is pspgl's. */
        s_state = 1;
        return s_state;
    }

    {
        char *p = (char *) &s_reservation;
        for (i = 0; i < (int) sizeof(s_reservation); ++i) {
            p[i] = 0;
        }
    }
    s_reservation.refcount = 1;
    s_reservation.flags    = M3G_BF_PINNED_FIXED | M3G_BF_UNMANAGED;
    s_reservation.size     = (int) used;

    /* First fit into an empty map: base comes back as edram + 0 and the block
     * covers precisely what PSPKVM took.  Rounding is pspgl's business; it
     * pads up to a cache line, which only ever grows the protected region. */
    s_state = __pspgl_vidmem_alloc(&s_reservation) ? 1 : -1;
    return s_state;
}

M3Gint m3gPspGetReservedVram(void)
{
    return (s_state == 1) ? s_reservation.size : 0;
}
