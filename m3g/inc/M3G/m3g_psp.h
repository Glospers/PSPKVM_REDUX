/*
 * m3g_psp.h -- the platform-facing entry points of the M3G port.
 *
 * m3gcore is handle based and stateless about who owns what: an application
 * creates an M3GInterface (the heap + callback bundle every object hangs off),
 * then drives an M3GLoader by hand -- feed bytes, check for a raised error,
 * collect the unreferenced objects, take a reference on each of them, destroy
 * the loader.  Getting that dance wrong leaks the whole scene or frees it out
 * from under the caller, so it is written exactly once, here, and both users
 * call it:
 *
 *   - jsr184/src/native/m3g_loader_kni.c, the KNI natives behind
 *     javax.microedition.m3g.Loader; and
 *   - test/m3g_load_test.c, the host-side harness that parses a real .m3g out
 *     of a game JAR without a PSP in the loop.
 *
 * Handles are returned as M3GObject (a pointer).  The Java side stores them in
 * an int field, which is sound here because inc/m3g_defs.h:609 already asserts
 * sizeof(M3Guint) >= sizeof(void*) for every configuration m3gcore supports.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#ifndef __M3G_PSP_H__
#define __M3G_PSP_H__

#include "M3G/m3g_core.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
 * Failure codes
 *
 * Returned by m3gPspLoadFromMemory in place of an object count, so they are
 * all negative.  They are mirrored by the M3G_LOAD_* constants in
 * javax.microedition.m3g.Loader, which turns them into the exception the
 * JSR-184 spec asks for.
 *--------------------------------------------------------------------*/

#define M3G_PSP_ERR_INVALID       (-1)  /* null/empty input               */
#define M3G_PSP_ERR_NO_INTERFACE  (-2)  /* m3gCreateInterface failed      */
#define M3G_PSP_ERR_OUT_OF_MEMORY (-3)  /* engine reported OOM            */
#define M3G_PSP_ERR_IO            (-4)  /* malformed or truncated file    */
#define M3G_PSP_ERR_UNSUPPORTED   (-5)  /* PNG identifier, or no roots    */
#define M3G_PSP_ERR_RENDER        (-6)  /* engine refused a draw call     */
#define M3G_PSP_ERR_BOUND         (-7)  /* a target is already bound      */
#define M3G_PSP_ERR_NOT_BOUND     (-8)  /* no target bound                */

/*! \brief The success code of the rendering entry points below. */
#define M3G_PSP_RENDER_OK         0

/*!
 * \brief The process-wide M3G interface, created on first use.
 *
 * There is deliberately one for the whole VM: engine objects belong to the
 * interface that created them, and objects from two different interfaces
 * cannot be linked into the same scene graph.
 *
 * CREATING IT IS NOT CHEAP, AND NOT ALWAYS SAFE
 *
 * m3gCreateInterface ends in m3gConfigureGL (m3gcore/src/m3g_interface.c:1679),
 * which brings EGL up, makes a 2x2 pbuffer current and reads the driver's
 * limits back off it -- so the first call to this function starts pspgl and
 * takes the video memory reservation with it.  That must not happen while
 * PSPKVM is drawing its own 2D: pspgl and PSPKVM's blit (lcd.c) both drive the
 * GE, and bringing the renderer up underneath the UI kills the display for
 * good -- the symptom is a MIDlet frozen on its loading screen with the
 * animation stopped, not just the game logic.
 *
 * So this is called from exactly two places, both of which were already doing
 * it before the M3G object layer existed and are therefore known to be safe:
 * binding a render target (m3gPspBindMemoryTarget) and parsing a file
 * (m3gPspLoadFromMemory).  Anything that merely *creates an object* must use
 * m3gPspPeekInterface instead.
 *
 * \return the interface, or NULL if it could not be created
 */
M3GInterface m3gPspGetInterface(void);

/*!
 * \brief The interface if it already exists, without ever creating one.
 *
 * The accessor for code that needs an interface but must not be the thing that
 * starts the renderer -- in practice every m3gCreate* behind a public
 * javax.microedition.m3g constructor.  A NULL return means "not up yet", and
 * the caller must skip the creation rather than pass NULL to the engine, which
 * would dereference it.
 *
 * \return the interface, or NULL if it has not been created yet
 */
M3GInterface m3gPspPeekInterface(void);

/*!
 * \brief Everything one parse produced.
 *
 * The loader is part of the result, and outliving the parse is the point of
 * it.  A .m3g file may attach user parameters -- arbitrary (id, bytes) pairs --
 * to any object in it, and the engine keeps them in the *loader*, not in the
 * objects: m3gGetObjectsWithUserParameters and its two companions all take an
 * M3GLoader and read a table that m3gCleanupLoader frees
 * (m3gcore/src/m3g_loader.c:2980-3050, :2723).  Destroying the loader as soon
 * as the parse finished therefore threw the parameters away before anyone
 * could read them, which is exactly what JSR-184 requires Loader.load to hand
 * back as each object's user object.
 *
 * So the loader now lives until the caller says it is finished with the
 * result.  Every field below is owned by the result and is not valid after
 * either release function has run.
 */
typedef struct {
    M3GLoader  loader;      /*!< the parse, kept alive for the fields below  */
    M3GObject *roots;       /*!< the objects nothing else in the file refers
                             *   to, one reference held on each              */
    M3Gint     rootCount;
    M3GObject *userObjects; /*!< the objects carrying user parameters, in the
                             *   order the accessors below index them        */
    M3Gint     userCount;
} M3GPspLoadResult;

/*!
 * \brief Parses one .m3g image out of memory.
 *
 * \param data   the file bytes
 * \param length how many of them
 * \param result receives the roots, the user-parameter table and the loader
 *               that owns both.  Release with m3gPspFinishResult once the
 *               roots have been handed on, or m3gPspReleaseResult to throw the
 *               whole scene away.
 *
 * \return the number of roots (>= 0), or one of M3G_PSP_ERR_* on failure, in
 *         which case the result is emptied and nothing needs releasing.
 */
M3Gint m3gPspLoadFromMemory(const M3Gubyte *data,
                            M3Gsizei length,
                            M3GPspLoadResult *result);

/*!
 * \brief Throws the whole result away, scene included.
 *
 * Every root goes to refcount zero and takes the scene graph hanging off it
 * along, so this is the "the load was for nothing" path.
 */
void m3gPspReleaseResult(M3GPspLoadResult *result);

/*!
 * \brief Releases the result but keeps the objects alive.
 *
 * The counterpart of m3gPspReleaseResult for when ownership of the references
 * has been handed to somebody else -- in practice, to the Java wrappers the
 * KNI layer built around the handles.
 */
void m3gPspFinishResult(M3GPspLoadResult *result);

/*!
 * \brief Number of user parameters carried by \a object.
 *
 * \param object index into result->userObjects, not a handle
 */
M3Gint m3gPspGetUserParamCount(const M3GPspLoadResult *result, M3Gint object);

/*! \brief Length in bytes of parameter \a index of \a object. */
M3Gint m3gPspGetUserParamLength(const M3GPspLoadResult *result,
                                M3Gint object, M3Gint index);

/*!
 * \brief Copies parameter \a index of \a object into \a buffer.
 *
 * \a buffer must have room for m3gPspGetUserParamLength bytes.
 *
 * \return the parameter's id
 */
M3Gint m3gPspGetUserParam(const M3GPspLoadResult *result,
                          M3Gint object, M3Gint index, void *buffer);

/*! \brief m3gGetClass, but tolerant of a NULL handle (returns -1). */
M3Gint m3gPspGetClassID(M3GObject object);

/*----------------------------------------------------------------------
 * Rendering (src/m3g_psp_render.c)
 *
 * The counterpart of the loader entry points above: the sequence m3gcore
 * expects around a frame, written once so the KNI natives behind
 * javax.microedition.m3g.Graphics3D stay pure marshalling.
 *
 * The target is always a *memory* target -- MIDP's 16-bit screen buffer --
 * because PSPKVM owns the scanout and blits that buffer itself every flush.
 * m3gcore renders into an offscreen pbuffer and reads back with glReadPixels,
 * so the 3D composes underneath whatever 2D the MIDlet draws afterwards.
 * See the header comment of src/m3g_psp_render.c.
 *
 * Every entry point returns M3G_PSP_RENDER_OK or one of the M3G_PSP_ERR_*
 * codes above, except where noted.
 *--------------------------------------------------------------------*/

/*!
 * \brief The process-wide rendering context, created on first use.
 *
 * \return the context, or NULL if it or the interface could not be created
 */
M3GRenderContext m3gPspGetContext(void);

/*!
 * \brief Binds a 16-bit RGB565 pixel buffer as the rendering target.
 *
 * \param pixels      first pixel, top-left
 * \param width       target width in pixels
 * \param height      target height in pixels
 * \param strideBytes distance between scanlines, >= width * 2
 * \param depthBuffer non-zero to request a depth buffer
 * \param hints       the JSR-184 Graphics3D hint bits, which are the same
 *                    values as the engine's mode bits
 */
M3Gint m3gPspBindMemoryTarget(void *pixels,
                              M3Gint width, M3Gint height,
                              M3Gint strideBytes,
                              M3Gint depthBuffer,
                              M3Gint hints);

/*!
 * \brief Releases the target, reading the rendered frame back into it.
 *
 * Nothing is written into the caller's buffer until this is called.
 */
M3Gint m3gPspReleaseTarget(void);

/*! \brief Non-zero while a target is bound. */
M3Gint m3gPspIsBound(void);

/*!
 * \brief Non-zero once a render target has been bound at least once.
 *
 * The gate for building engine objects at all -- a different question from
 * m3gPspIsBound, which asks whether one is bound right now.
 *
 * Creating an Image2D commits it, and committing uploads its texture there and
 * then through GL (m3gcore/src/m3g_image.inl:146, :157, :190). Doing that
 * before the MIDlet has ever entered a drawing window means driving the GE at
 * an arbitrary point in its startup, while PSPKVM is still painting its own 2D
 * with sceGu -- two users of one engine that do not coordinate. Objects a
 * MIDlet constructs earlier than that are queued on the Java side and built by
 * Object3D.flushDeferred at the first bind.
 */
M3Gint m3gPspRendererReady(void);

/*! \brief Collects and clears the engine's error code. */
M3Gint m3gPspTakeError(void);

/*!
 * \brief Announces the frame the engine is about to read back.
 *
 * m3gcore reads a finished frame out of GL in dozens of small chunks, each of
 * which costs a full GE synchronisation.  The read-back corrector in
 * src/m3g_psp_gl.c uses this hint to fetch the whole frame in one transfer
 * and serve the chunks from memory.  Called by m3gPspReleaseTarget; anything
 * else driving m3gReleaseTarget by hand should do the same.
 */
void m3gPspReadbackHint(M3Gint x, M3Gint y, M3Gint width, M3Gint height);

/*!
 * \brief Writes the frame just read back straight into a MIDP 565 buffer.
 *
 * The cached frame is already in the screen's own pixel layout, so this is a
 * row-wise copy -- it replaces expanding every chunk to RGBA8, the engine's
 * copy of those chunks, and packing the result back down to 565.
 *
 * \return non-zero if the frame was delivered; zero if the caller must fall
 *         back to converting the engine's staging buffer.
 */
M3Gint m3gPspFrameToMidp(unsigned short *dst, M3Gint width, M3Gint height);

/*!
 * \brief Checks the direct copy against a frame the engine delivered.
 *
 * Enables m3gPspFrameToMidp only if the copy reproduces \a reference
 * exactly. Call once, with the first frame the proven path produced.
 */
void m3gPspFrameVerify(const unsigned short *reference,
                       M3Gint width, M3Gint height);

void   m3gPspSetViewport(M3Gint x, M3Gint y, M3Gint width, M3Gint height);
void   m3gPspSetClipRect(M3Gint x, M3Gint y, M3Gint width, M3Gint height);
void   m3gPspSetDepthRange(M3Gfloat depthNear, M3Gfloat depthFar);

/*! \brief Clears the viewport; \a background may be NULL. */
M3Gint m3gPspClear(M3GObject background);

/*!
 * \brief Renders a World with its own active camera, lights and background.
 *
 * This needs nothing from the Java side but the handle: the engine holds the
 * whole scene graph internally.
 */
M3Gint m3gPspRenderWorld(M3GObject world);

/*! \brief Renders a subtree; \a transform is 16 floats row-major, or NULL. */
M3Gint m3gPspRenderNode(M3GObject node, const M3Gfloat *transform);

/*! \brief Immediate-mode submission; \a appearance may be NULL. */
M3Gint m3gPspRenderImmediate(M3GObject vertices,
                             M3GObject triangles,
                             M3GObject appearance,
                             const M3Gfloat *transform,
                             M3Gint scope);

/*! \brief Sets the camera; \a camera may be NULL to unset it. */
M3Gint m3gPspSetCamera(M3GObject camera, const M3Gfloat *transform);

/*! \brief Adds a light; returns its index (>= 0) or an M3G_PSP_ERR_* code. */
M3Gint m3gPspAddLight(M3GObject light, const M3Gfloat *transform);

void   m3gPspClearLights(void);

/*----------------------------------------------------------------------
 * Video memory (src/m3g_psp_vidmem.c)
 *--------------------------------------------------------------------*/

/*!
 * \brief Reserves edram so pspgl cannot allocate over PSPKVM's frame buffers.
 *
 * By default it reserves all of it, which also keeps pspgl off its own
 * eviction path -- that path emits GE commands through a context that is not
 * current yet during target binding, and faults.  Override with
 * -DM3G_PSP_VRAM_RESERVE_KB=<n>, but read the note at the top of
 * src/m3g_psp_vidmem.c first; the value is clamped up to PSPKVM's high-water
 * mark and down to the size of edram either way.
 *
 * Idempotent, and called from m3gPspGetInterface before anything can touch
 * EGL.  Returns 1 if the reservation is held, -1 if pspgl refused it.
 */
M3Gint m3gPspReserveVram(void);

/*! \brief Bytes of edram reserved for PSPKVM, or 0 if the reservation failed. */
M3Gint m3gPspGetReservedVram(void);

/*!
 * \brief Brings EGL up and keeps a context current for the whole process.
 *
 * pspgl dereferences its current-context global on every path that reaches the
 * GE, without checking it -- so any GL call made while no context is current
 * writes through a null display list. m3gcore only keeps a context current
 * inside a bound target, yet touches GL well outside one: committing an
 * Image2D uploads a texture there and then (m3gcore/src/m3g_image.inl:146).
 * Holding a context of our own removes that whole class of fault.
 *
 * Must be called *before* m3gCreateInterface, so that m3gcore sees EGL already
 * initialised and takes the reference that stops its own probe from tearing it
 * down again (m3gcore/src/m3g_interface.c:1382). Idempotent: later calls just
 * re-assert the context as current.
 *
 * \return 1 if a context is current, -1 if EGL refused
 */
int m3gPspHoldGLContext(void);

/*----------------------------------------------------------------------
 * C heap probe (diagnostic -- see src/m3g_psp_heapcheck.c)
 *
 * pspgl was handed a pointer 3.7 MB below the heap by memalign and zeroed
 * 2336 bytes at it, which is what wiped lcd.c's statics and crashed the blit.
 * An allocator only does that once its metadata has been overwritten, so the
 * heap is already broken before pspgl asks.  These two report *when* it broke.
 *
 * Note that the Java object heap is itself one huge malloc'd block (see the
 * comment below), so the VM writing past its own heap would damage exactly the
 * libc metadata in question -- which is why the probe is called around the
 * phases that allocate Java objects heavily, not only around engine calls.
 *--------------------------------------------------------------------*/

void m3gPspHeapCheck(const char *tag);
void m3gPspHeapReport(void);

/*----------------------------------------------------------------------
 * The engine's private heap
 *
 * m3gcore does not allocate from the C heap.  It cannot: on PSPKVM the Java
 * object heap is itself one huge malloc'd block (javacall_memory_heap_allocate,
 * javacall/implementation/psp_mips/common/memory.c:48), so engine blocks would
 * sit directly against VM memory and an overrun either way would be invisible
 * until the VM failed somewhere unrelated.  Instead every M3Gparams callback
 * routes to src/m3g_psp_arena.c, a fixed static heap in .bss with guard bands
 * and per-block canaries.
 *
 * The entry points below exist so the KNI layer can report what the arena saw
 * after each load; see jsr184/src/native/m3g_loader_kni.c.
 *--------------------------------------------------------------------*/

/*! \brief Verification results, also used as the sticky fault code. */
#define M3G_PSP_ARENA_OK           0
#define M3G_PSP_ARENA_HEAD_GUARD   1  /* wrote before the first block   */
#define M3G_PSP_ARENA_TAIL_GUARD   2  /* wrote past the last block      */
#define M3G_PSP_ARENA_BAD_HEADER   3  /* block header magic destroyed   */
#define M3G_PSP_ARENA_BAD_FOOTER   4  /* block overran its own payload  */
#define M3G_PSP_ARENA_BAD_CHAIN    5  /* prev-size links inconsistent   */
#define M3G_PSP_ARENA_DOUBLE_FREE  6
#define M3G_PSP_ARENA_FOREIGN_PTR  7  /* free() of a non-arena pointer  */

typedef struct {
    M3Gint capacity;   /* usable bytes in the arena                       */
    M3Gint used;       /* payload bytes currently handed out              */
    M3Gint peak;       /* high-water mark of the above                    */
    M3Gint blocks;     /* live blocks                                     */
    M3Gint failures;   /* allocations the arena could not serve           */
    M3Gint corrupt;    /* canary violations seen so far                   */
    M3Gint firstBad;   /* address of the first block found corrupt        */
    M3Gint fault;      /* M3G_PSP_ARENA_* code of that first violation    */
} M3GPspArenaStats;

/*! \brief Allocates from the engine arena.  The M3Gparams mallocFunc. */
void *m3gPspArenaAlloc(M3Guint bytes);

/*! \brief Returns a block to the engine arena.  The M3Gparams freeFunc. */
void m3gPspArenaFree(void *ptr);

/*!
 * \brief Walks every guard band, block header and block canary.
 *
 * \return M3G_PSP_ARENA_OK, or the first M3G_PSP_ARENA_* fault found.
 */
M3Gint m3gPspArenaVerify(void);

/* TEMPORARY -- the poisoned-parent hunt; see m3g_psp_arena.c. */
void   m3gPspArenaAuditNodes(const void *iface, const char *when);
M3Gint m3gPspArenaPointerOk(const void *p);

/*! \brief Copies out the arena counters.  Never fails. */
void m3gPspArenaGetStats(M3GPspArenaStats *out);

#if defined(__cplusplus)
}
#endif

#endif /* __M3G_PSP_H__ */
