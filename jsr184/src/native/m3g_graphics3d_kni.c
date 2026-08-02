/*
 * m3g_graphics3d_kni.c -- the natives behind javax.microedition.m3g.Graphics3D.
 *
 * The naming rules and the KNI parameter convention are the same as in
 * m3g_loader_kni.c, which this file follows: phoneME binds natives statically
 * from the class and method name alone (cldc/src/vm/share/ROM/
 * SourceObjectWriter.cpp:648), so there is no registration step and nothing to
 * keep in sync but the spelling below.  All of these are declared static on
 * the Java side, so KNI parameter index 1 is the first argument.
 *
 * The engine sequencing lives in m3g/src/m3g_psp_render.c; this file is
 * marshalling and one platform lookup.
 *
 * WHERE THE PIXELS GO
 *
 * PSPKVM owns the screen.  It sets the GE up itself (psp/pspkvm.c:364-392) and
 * blits the MIDP 16-bit screen buffer as a textured quad on every flush
 * (javacall/implementation/psp_mips/midp/lcd.c:121-138), ending in
 * sceGuSwapBuffers().  So the 3D must not go to the display: it goes into that
 * same 16-bit buffer, underneath whatever 2D the MIDlet draws afterwards, and
 * PSPKVM's existing blit puts the composite on screen with no change at all.
 *
 * That buffer is the one MIDP itself draws into -- jcapp_export.c:47 sets
 * gxj_system_screen_buffer.pixelData from javacall_lcd_get_screen(), so the
 * lcdui Graphics and this file are writing to the same pixels.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include <stddef.h>     /* NULL -- kni.h does not pull in a libc header */
#include <stdio.h>      /* sprintf, for the diagnostic line below       */
#include <string.h>     /* memcpy, staging the frame in and out         */
#include <stdlib.h>     /* free -- see m3gStageBuffer                   */
#include <malloc.h>     /* memalign -- ditto                            */

#include <kni.h>
#include <sni.h>

#include "M3G/m3g_psp.h"

/*----------------------------------------------------------------------
 * The MIDP screen buffer
 *
 * javacall_lcd_get_screen is declared here rather than through
 * <javacall_lcd.h> because that header lives in the javacall tree, which is
 * built after this one and is not on MIDP's native include path.  The
 * prototype is javacall/interface/midp/javacall_lcd.h:173 with its two enum
 * parameters spelled as the int they are passed as under o32; the two
 * constants are javacall_lcd.h:100 and :83.
 *--------------------------------------------------------------------*/

#define JAVACALL_LCD_SCREEN_PRIMARY  1600
#define JAVACALL_LCD_COLOR_RGB565     200

extern unsigned short *javacall_lcd_get_screen(int screenType,
                                               int *screenWidth,
                                               int *screenHeight,
                                               int *colorEncoding);

/*----------------------------------------------------------------------
 * The MIDlet's rendering target
 *
 * bindTarget takes a Graphics, and a Graphics does not necessarily draw to the
 * display: GameCanvas hands out one that draws into a private off-screen Image
 * and only reaches the screen when the MIDlet calls flushGraphics
 * (midp/src/highlevelui/lcdui/reference/classes/.../game/GameCanvas.java:188).
 * Every title that uses GameCanvas -- which is most of them that draw 3D --
 * therefore has a target that is NOT the screen buffer, and rendering into the
 * screen buffer regardless meant the MIDlet's own flush overwrote the 3D with
 * its untouched off-screen buffer a moment later. The symptom is a black
 * screen with a rendering loop that reports success on every call.
 *
 * These three headers are what MIDP's own drawing natives use to answer the
 * same question (gxapi_graphics_kni.c:46): a Graphics carries the Image it
 * draws into, an Image carries an ImageData, and an ImageData carries either a
 * Java byte[] of 16-bit pixels or a native block. Both are RGB565, which is
 * what the engine's memory target wants.
 *--------------------------------------------------------------------*/

#include <gxapi_graphics.h>     /* java_graphics, GXAPI_GET_GRAPHICS_PTR   */
#include <imgapi_image.h>       /* java_imagedata                          */
#include <gxj_putpixel.h>       /* gxj_screen_buffer + the accessor        */

/* As gxapi_graphics_kni.c:44, which does not export it. A Graphics with no
 * Image behind it is one that draws straight to the display. */
#define M3G_IMAGEDATA_OF(handle)                        \
    (GXAPI_GET_GRAPHICS_PTR(handle)->img != NULL        \
     ? GXAPI_GET_GRAPHICS_PTR(handle)->img->imageData   \
     : (java_imagedata *) NULL)

/*----------------------------------------------------------------------
 * Diagnostics
 *
 * Same sink as the loader natives: ms0:/pspkvm_vm.log, weak so that a build
 * without docker/patches/0043 -- and the romgen host tool, which has no
 * javacall at all -- still links.
 *--------------------------------------------------------------------*/

extern void javacall_diag_log(const char *s) __attribute__((weak));

static void m3gLog(const char *what, jint a, jint b)
{
    char line[128];

    if (javacall_diag_log == 0) {
        return;
    }
    sprintf(line, "M3G: %s %d %d\n", what, (int) a, (int) b);
    javacall_diag_log(line);
}

/*
 * MILESTONES
 *
 * Every log line is an open/write/close on the memory stick, and these entry
 * points run per frame -- so a per-call trace here is a frame-rate experiment,
 * not an observation.  What is actually worth knowing is binary and happens
 * once: did a target ever bind, did a camera with a real handle ever reach the
 * context, did a node ever draw, and did drawing ever fail.  Each of those is
 * reported the first time it happens and never again, which is four writes for
 * a whole session.
 *
 * Build with -DM3G_TRACE for the old per-call trace instead.
 */
#define M3G_MILESTONE_BIND        0
#define M3G_MILESTONE_CAMERA      1
#define M3G_MILESTONE_RENDER_OK   2
#define M3G_MILESTONE_RENDER_FAIL 3
#define M3G_MILESTONE_COUNT       4

static unsigned char s_milestone[M3G_MILESTONE_COUNT];

/*! \brief Logs \a what the first time this milestone is reached. */
static void m3gMilestone(int which, const char *what, jint a, jint b)
{
    if (s_milestone[which]) {
        return;
    }
    s_milestone[which] = 1;
    m3gLog(what, a, b);
}

/*
 * FRAME EVENTS
 *
 * Bind and draw calls, entry and result, for the first few frames only.
 *
 * The milestones above say whether something ever happened; they cannot say
 * where a stall is, because a call that never returns never reaches its
 * milestone. These do: an entry line with no matching result line is a call
 * that did not come back, which is precisely the distinction between dying
 * before a draw and dying inside one.
 *
 * Budgeted rather than switched, because these run per frame and per node --
 * roughly twenty lines a frame for this title -- and javacall_diag_log costs a
 * whole open/write/close each time. Three frames' worth is enough to see the
 * shape of the first frame and cheap enough not to be the thing being
 * measured; after that it goes quiet on its own.
 */
#define M3G_EVENT_BUDGET 60
static int s_eventLeft = M3G_EVENT_BUDGET;

static void m3gEvent(const char *what, jint a, jint b)
{
    if (s_eventLeft <= 0) {
        return;
    }
    --s_eventLeft;
    m3gLog(what, a, b);
}

#if defined(M3G_PSP_FRAME_TIMING)
/*
 * FRAME TIMING -- where the milliseconds actually go.
 *
 * Enabled from jsr184/src/config/subsystem.gmk.  Microsecond stamps are
 * accumulated around the four stages this layer controls and reported once
 * every 128 frames, so the measurement costs one log line per two seconds of
 * play rather than perturbing the thing being measured:
 *
 *   frame   -- bind to bind, i.e. everything including the MIDlet's own Java
 *   seed    -- packing the target's 2D content into the engine's staging buffer
 *   bind    -- m3gPspBindMemoryTarget
 *   render  -- release entry minus bind exit: the engine's draws plus all the
 *              Java the MIDlet ran between them
 *   read    -- m3gPspReleaseTarget, which is the GPU read-back and its sync
 *   deliver -- getting the finished frame into MIDP's buffer
 *
 * Whatever `frame` exceeds the sum of the rest is the VM: interpreted
 * bytecode, MIDP's own 2D, and PSPKVM's blit.
 */
extern unsigned int sceKernelGetSystemTimeLow(void);

static unsigned int s_tSeed, s_tBind, s_tRender, s_tRead, s_tDeliver, s_tFrame;
static unsigned int s_tLastBind, s_tBindExit;
static int s_timedFrames;

static void m3gFrameReport(void)
{
    char line[160];

    if (++s_timedFrames < 128 || javacall_diag_log == 0) {
        return;
    }
    sprintf(line, "M3G: t frame=%u seed=%u bind=%u render=%u read=%u"
            " deliver=%u (us/frame)\n",
            s_tFrame / 128u, s_tSeed / 128u, s_tBind / 128u,
            s_tRender / 128u, s_tRead / 128u, s_tDeliver / 128u);
    javacall_diag_log(line);
    s_timedFrames = 0;
    s_tSeed = s_tBind = s_tRender = s_tRead = s_tDeliver = s_tFrame = 0;
}
#endif /* M3G_PSP_FRAME_TIMING */

#if defined(M3G_TRACE)

#define M3G_TRACE_BUDGET 200
static int s_traceLeft = M3G_TRACE_BUDGET;

static void m3gTraceImpl(const char *what, jint a, jint b)
{
    if (s_traceLeft <= 0) {
        return;
    }
    --s_traceLeft;
    m3gLog(what, a, b);
}

#define m3gTrace(what, a, b) m3gTraceImpl((what), (a), (b))

#else

#define m3gTrace(what, a, b) ((void) 0)

#endif /* M3G_TRACE */

/*----------------------------------------------------------------------
 * Transform marshalling
 *
 * JSR-184 Transform.get() yields sixteen floats in row-major order, which is
 * what m3gSetMatrixRows expects (m3g/src/m3g_psp_render.c does that half).
 * The array is copied out rather than passed through, because the caller of a
 * native may not assume the Java heap holds still: SNI_GetRawArrayPointer
 * hands out an address inside it, and the engine calls below allocate from the
 * M3G arena and can run for a long time.
 *--------------------------------------------------------------------*/

#define M3G_MATRIX_FLOATS 16

static const M3Gfloat *m3gFetchTransform(jobject array, M3Gfloat *out)
{
    const M3Gfloat *src;
    int i;

    if (KNI_IsNullHandle(array) || KNI_GetArrayLength(array) < M3G_MATRIX_FLOATS) {
        return NULL;
    }
    src = (const M3Gfloat *) SNI_GetRawArrayPointer(array);
    if (src == NULL) {
        return NULL;
    }
    for (i = 0; i < M3G_MATRIX_FLOATS; ++i) {
        out[i] = src[i];
    }
    return out;
}

/*----------------------------------------------------------------------
 * Natives
 *--------------------------------------------------------------------*/

/*----------------------------------------------------------------------
 * Staging the frame
 *
 * The engine's target has to be a fixed address: it is handed over at bind and
 * not touched again until release, and in between the MIDlet runs. A target
 * whose pixels are a Java byte[] -- which is what an off-screen Image is under
 * the putpixel port (ImageData.java:49) -- can be moved by a garbage
 * collection in that window, leaving the engine to write the finished frame
 * over whatever now lives at the old address.
 *
 * So the engine always renders into the MIDP screen buffer, which is native
 * memory at a fixed address, and the MIDlet's real target is copied in at bind
 * and out at release. Both copies happen inside a native call, which is the
 * only place a pointer into the Java heap is allowed to exist.
 *
 * Copying in as well as out matters: unless the OVERWRITE hint is given the
 * engine seeds its back buffer from the target, which is how a MIDlet that
 * turns colour clearing off gets its 3D composited over 2D it drew first.
 * Deep 3D does exactly that (Background.setColorClearEnable(false)).
 *
 * When the target IS the display -- a plain Canvas -- source and destination
 * are the same memory and both copies are skipped.
 *--------------------------------------------------------------------*/

/*!
 * \brief The pixels behind a MIDlet's rendering target.
 *
 * \param graphics the Object handed to bindTarget
 * \param out      receives width, height and the pixel pointer
 * \return non-zero if a usable destination was found
 */
static int m3gTargetBuffer(jobject graphics, gxj_screen_buffer *out)
{
    java_imagedata *data;
    int isGraphics;

    if (KNI_IsNullHandle(graphics)) {
        return 0;
    }

    /* The macro below reads the object as a Graphics without asking, which is
     * safe for MIDP's own natives -- their signatures say Graphics -- and not
     * safe here, because bindTarget takes an Object and a MIDlet may hand over
     * anything at all. Reading some other class's fields as a Graphics would
     * follow whatever happens to sit where `img` belongs. */
    KNI_StartHandles(1);
    {
        KNI_DeclareHandle(graphicsClass);
        KNI_FindClass("javax/microedition/lcdui/Graphics", graphicsClass);
        isGraphics = !KNI_IsNullHandle(graphicsClass)
                  && KNI_IsInstanceOf(graphics, graphicsClass);
    }
    KNI_EndHandles();

    if (!isGraphics) {
        return 0;
    }

    data = M3G_IMAGEDATA_OF(graphics);
    if (data == NULL) {
        /* Draws straight to the display. */
        *out = gxj_system_screen_buffer;
    }
    else if (gxj_get_image_screen_buffer_impl(data, out, graphics) == NULL) {
        return 0;
    }

    return (out->pixelData != NULL && out->width > 0 && out->height > 0);
}

/*
 * The engine's render target: a buffer of our own, aligned for the GE.
 *
 * It used to be the MIDP screen buffer.  Two things were wrong with that.
 * The alignment one is why the frame came back black: pspgl's glReadPixels
 * has two paths, chosen by the *destination's* alignment -- 16-byte aligned
 * goes through __pspgl_copy_pixels, a GE transfer, and anything else through
 * a row-by-row CPU copy (disassembly of libGL.a, the branch at
 * glReadPixels+0x1dc).  The GE transfer is the one that works everywhere:
 * PPSSPP only materialises rendered VRAM into CPU-visible memory when the
 * copy goes through the GE, so the CPU path reads back zeros there no matter
 * what was drawn.  The screen buffer's alignment is whatever javacall chose;
 * this one is aligned by construction, and the GE path is also faster.
 *
 * The other problem was sharing: PSPKVM's blit reads the screen buffer on its
 * own schedule, so the engine could be writing a frame into it mid-blit.  A
 * private buffer ends that too.
 */
/* The stage is RGBA8, not 565: the engine's blit of prior target content
 * into the frame -- the path 2D-under-3D and frame erasure both live on --
 * has no 565 case and drops the whole copy on the floor, and its 565
 * readback packs red into the wrong end for the PSP.  See the note at
 * m3gPspBindMemoryTarget.  The conversions live here, at the two spots the
 * pixels cross between MIDP's buffer and the engine's. */
static unsigned int *s_stage;
static int s_stageCapacity;

static unsigned int *m3gStageBuffer(int width, int height)
{
    int need = width * height;

    if (need > s_stageCapacity) {
        free(s_stage);
        /* 64: the dcache line, so the GE copy and the CPU never split a
         * line; anything >= 16 takes the GE path. */
        s_stage = (unsigned int *) memalign(64, (size_t) need * 4);
        s_stageCapacity = (s_stage != NULL) ? need : 0;
    }
    return s_stage;
}

/*
 * MIDP 565 (PSP order: red low) -> the engine's RGBA8 (byte order R,G,B,A,
 * which is the word 0xAABBGGRR on this little-endian CPU).
 *
 * Table-driven: the straightforward version costs about fifteen operations
 * per pixel and runs over the whole frame at every bind, which is
 * milliseconds of pure CPU on this hardware.  Two 256-entry tables split the
 * 16-bit source into its high and low bytes, so the inner loop is two loads
 * and an or.  Built on first use; 2 KB total.
 */
static unsigned int s_seedLo[256], s_seedHi[256];
static int s_seedTablesReady;

static void m3gBuildSeedTables(void)
{
    int v;

    /*
     * Source pixel (PSP order): red = bits 0-4, green = 5-10, blue = 11-15.
     * Split at the byte boundary, so red comes entirely from the low byte,
     * blue entirely from the high byte, and green from both:
     *     green6 = gLow | (gHigh << 3),  gLow = (lo >> 5) & 7, gHigh = hi & 7
     * The 8-bit expansion (g << 2) | (g >> 4) lands in disjoint bit ranges
     * for the two halves -- gHigh >> 1 in bits 0-1, gLow << 2 in bits 2-4,
     * gHigh << 5 in bits 5-7 -- so OR-ing the two table entries reproduces
     * it exactly rather than approximately.
     */
    for (v = 0; v < 256; ++v) {
        unsigned int r5   = (unsigned int) (v & 0x1F);
        unsigned int gLow = (unsigned int) ((v >> 5) & 0x07);
        unsigned int r8   = (r5 << 3) | (r5 >> 2);

        s_seedLo[v] = 0xFF000000u | r8 | ((gLow << 2) << 8);
    }
    for (v = 0; v < 256; ++v) {
        unsigned int gHigh = (unsigned int) (v & 0x07);
        unsigned int b5    = (unsigned int) ((v >> 3) & 0x1F);
        unsigned int b8    = (b5 << 3) | (b5 >> 2);

        s_seedHi[v] = (b8 << 16) | ((((gHigh << 5) | (gHigh >> 1))) << 8);
    }
    s_seedTablesReady = 1;
}

static void m3gStageSeed(unsigned int *dst, const unsigned short *src, int count)
{
    int i;

    if (!s_seedTablesReady) {
        m3gBuildSeedTables();
    }
    for (i = 0; i < count; ++i) {
        unsigned int v = src[i];
        dst[i] = s_seedLo[v & 0xFF] | s_seedHi[v >> 8];
    }
}

/* The engine's RGBA8 back into MIDP's PSP-order 565.
 *
 * Measured, not assumed: pspgl's glReadPixels hands the frame back with red
 * and blue exchanged (the 16-bit pbuffer is PSP-order, red low, and the
 * unpack reads it GL-order), while its glTexImage2D upload direction is
 * true-order -- which is why the seed above converts straight and this one
 * compensates.  The old all-565 pipeline was colour-correct only because
 * m3gcore's own GL-order 565 packing swapped everything a second time. */
static void m3gStageDeliver(unsigned short *dst, const unsigned int *src, int count)
{
    int i;
    for (i = 0; i < count; ++i) {
        unsigned int w = src[i];
        dst[i] = (unsigned short) (((w & 0x00F80000u) >> 19)
                                 | ((w & 0x0000FC00u) >> 5)
                                 | ((w & 0x000000F8u) << 8));
    }
}

/*
 * private static native int nBind(Object target, int hints, int depthBuffer);
 *
 * Binds the MIDlet's target for rendering.  Returns its size packed as
 * (width << 16) | height so that the Java side can set the default viewport
 * the specification asks for, or a negative M3G_PSP_ERR_*.
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Graphics3D_nBind()
{
    jint hints       = KNI_GetParameterAsInt(2);
    jint depthBuffer = KNI_GetParameterAsInt(3);

#if defined(M3G_PSP_FRAME_TIMING)
    unsigned int tEntry = sceKernelGetSystemTimeLow();
    unsigned int tSeeded = tEntry;

    if (s_tLastBind != 0) {
        s_tFrame += tEntry - s_tLastBind;
    }
    s_tLastBind = tEntry;
#endif

    unsigned short *pixels;
    unsigned short *stage;
    int screenWidth = 0, screenHeight = 0, encoding = 0;
    jint result = M3G_PSP_ERR_UNSUPPORTED;
    int width = 0, height = 0;

    pixels = javacall_lcd_get_screen(JAVACALL_LCD_SCREEN_PRIMARY,
                                     &screenWidth, &screenHeight, &encoding);

    if (pixels == NULL || screenWidth <= 0 || screenHeight <= 0
        || encoding != JAVACALL_LCD_COLOR_RGB565) {
        m3gLog("bind no-screen", screenWidth, screenHeight);
        KNI_ReturnInt(M3G_PSP_ERR_UNSUPPORTED);
    }

    KNI_StartHandles(1);
    {
        KNI_DeclareHandle(target);
        gxj_screen_buffer dst;

        KNI_GetParameterAsObject(1, target);

        int resolved = m3gTargetBuffer(target, &dst);
        if (!resolved) {
            /* Not a Graphics, or one with nothing behind it: fall back to the
             * display, which is what this always used to do. */
            dst.width     = screenWidth;
            dst.height    = screenHeight;
            dst.pixelData = (gxj_pixel_type *) pixels;
        }

        /* The screen is the largest surface anything renders at. */
        if (dst.width > screenWidth || dst.height > screenHeight) {
            dst.width  = screenWidth;
            dst.height = screenHeight;
        }
        width  = dst.width;
        height = dst.height;

        stage = m3gStageBuffer(width, height);
        if (stage == NULL) {
            result = M3G_PSP_ERR_OUT_OF_MEMORY;
        }
        else {
            /* Seed the frame with the target's current content: unless the
             * OVERWRITE hint is set the engine composes the 3D over it,
             * which is how 2D drawn before bindTarget shows through.  This
             * title's warm radiation backdrop is exactly that, so the seed
             * is load-bearing -- see the note in m3gPspBindMemoryTarget. */
            m3gStageSeed(stage, (const unsigned short *) dst.pixelData,
                         width * height);
#if defined(M3G_PSP_FRAME_TIMING)
            tSeeded = sceKernelGetSystemTimeLow();
            s_tSeed += tSeeded - tEntry;
#endif

            result = m3gPspBindMemoryTarget(stage, width, height,
                                            width * (M3Gint) sizeof(unsigned int),
                                            depthBuffer, hints);
        }
    }
    KNI_EndHandles();

    if (result != M3G_PSP_RENDER_OK) {
        m3gLog("bind failed", result, 0);
        KNI_ReturnInt(result);
    }

    m3gMilestone(M3G_MILESTONE_BIND, "bind ok", width, height);
    m3gEvent("bind", width, height);
    m3gTrace("bind ok", width, height);

#if defined(M3G_PSP_FRAME_TIMING)
    s_tBindExit = sceKernelGetSystemTimeLow();
    s_tBind += s_tBindExit - tSeeded;
#endif

    KNI_ReturnInt((width << 16) | height);
}

/*
 * private static native int nRelease(Object target);
 *
 * Reads the rendered frame back out of the engine and into the MIDlet's target.
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Graphics3D_nRelease()
{
#if defined(M3G_PSP_FRAME_TIMING)
    unsigned int tEntry = sceKernelGetSystemTimeLow();

    if (s_tBindExit != 0) {
        s_tRender += tEntry - s_tBindExit;
    }
#endif

    /* This is what makes the frame appear: the engine reads its pbuffer back
     * into the staging buffer (through the GE -- see m3gStageBuffer), and the
     * result is copied into the pixels the MIDlet will flush. */
    jint result = m3gPspReleaseTarget();

#if defined(M3G_PSP_FRAME_TIMING)
    unsigned int tRead = sceKernelGetSystemTimeLow();

    s_tRead += tRead - tEntry;
#endif

    unsigned short *pixels;
    int screenWidth = 0, screenHeight = 0, encoding = 0;

    pixels = javacall_lcd_get_screen(JAVACALL_LCD_SCREEN_PRIMARY,
                                     &screenWidth, &screenHeight, &encoding);
    if (pixels == NULL || screenWidth <= 0 || screenHeight <= 0) {
        KNI_ReturnInt(result);
    }

    KNI_StartHandles(1);
    {
        KNI_DeclareHandle(target);
        gxj_screen_buffer dst;

        KNI_GetParameterAsObject(1, target);

        if (s_stage != NULL) {
            int width, height;

            if (!m3gTargetBuffer(target, &dst)) {
                /* The same fallback nBind used: the display itself. */
                dst.width     = screenWidth;
                dst.height    = screenHeight;
                dst.pixelData = (gxj_pixel_type *) pixels;
            }
            width  = (dst.width  > screenWidth)  ? screenWidth  : dst.width;
            height = (dst.height > screenHeight) ? screenHeight : dst.height;

            /* The fast path: the cached frame is already in the screen's
             * own pixel layout, so it is copied straight in and the whole
             * RGBA8 round trip is skipped.  It only engages once it has
             * been checked against a frame the conversion below produced --
             * m3gPspFrameVerify does that on the first frame. */
            if (!m3gPspFrameToMidp((unsigned short *) dst.pixelData,
                                   width, height)) {
                m3gStageDeliver((unsigned short *) dst.pixelData, s_stage,
                                width * height);
                m3gPspFrameVerify((const unsigned short *) dst.pixelData,
                                  width, height);
            }
        }
    }
    KNI_EndHandles();

#if defined(M3G_PSP_FRAME_TIMING)
    s_tDeliver += sceKernelGetSystemTimeLow() - tRead;
    m3gFrameReport();
#endif

    m3gTrace("release", result, 0);
    KNI_ReturnInt(result);
}

/*
 * private static native void nSetViewport(int x, int y, int width, int height);
 */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Graphics3D_nSetViewport()
{
    m3gPspSetViewport(KNI_GetParameterAsInt(1),
                      KNI_GetParameterAsInt(2),
                      KNI_GetParameterAsInt(3),
                      KNI_GetParameterAsInt(4));
    KNI_ReturnVoid();
}

/*
 * private static native void nSetClipRect(int x, int y, int width, int height);
 */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Graphics3D_nSetClipRect()
{
    m3gPspSetClipRect(KNI_GetParameterAsInt(1),
                      KNI_GetParameterAsInt(2),
                      KNI_GetParameterAsInt(3),
                      KNI_GetParameterAsInt(4));
    KNI_ReturnVoid();
}

/*
 * private static native void nSetDepthRange(float near, float far);
 */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Graphics3D_nSetDepthRange()
{
    m3gPspSetDepthRange((M3Gfloat) KNI_GetParameterAsFloat(1),
                        (M3Gfloat) KNI_GetParameterAsFloat(2));
    KNI_ReturnVoid();
}

/*
 * private static native int nClear(int backgroundHandle);
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Graphics3D_nClear()
{
    jint handle = KNI_GetParameterAsInt(1);
    jint result = m3gPspClear((M3GObject) handle);
    m3gTrace("clear", handle, result);
    KNI_ReturnInt(result);
}

/*
 * private static native int nRenderWorld(int worldHandle);
 *
 * The handle goes straight through: the engine owns the whole scene graph, so
 * this needs nothing from the Java-side wrappers.
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Graphics3D_nRenderWorld()
{
    jint handle = KNI_GetParameterAsInt(1);
    jint result;

    if (handle == 0) {
        m3gTrace("renderWorld nohandle", 0, 0);
        KNI_ReturnInt(M3G_PSP_ERR_INVALID);
    }
    m3gEvent("renderWorld>", handle, 0);
    result = m3gPspRenderWorld((M3GObject) handle);
    m3gEvent("renderWorld<", handle, result);
    m3gTrace("renderWorld", handle, result);
    KNI_ReturnInt(result);
}

/*
 * private static native int nRenderNode(int nodeHandle, float[] transform);
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Graphics3D_nRenderNode()
{
    jint handle = KNI_GetParameterAsInt(1);
    M3Gfloat matrix[M3G_MATRIX_FLOATS];
    const M3Gfloat *transform;
    jint result;

    if (handle == 0) {
        m3gTrace("renderNode nohandle", 0, 0);
        KNI_ReturnInt(M3G_PSP_ERR_INVALID);
    }

    /* Parent sanity guard: a node whose parent link is not a pointer into
     * the arena is skipped (and reported, budgeted) instead of letting the
     * engine's alignment update walk the poisoned chain and crash.  The
     * use-after-free family that produced such nodes is fixed (wrappers pin
     * their objects), so this is a cheap regression backstop. */
    {
        const void *parent = *(const void **)
            ((const char *) (size_t) handle + 0x3C);
        if (!m3gPspArenaPointerOk(parent)) {
            extern void javacall_diag_log(const char *s)
                __attribute__((weak));
            static int logged;
            if (logged < 8 && javacall_diag_log != 0) {
                char line[120];
                logged++;
                sprintf(line, "M3G: renderNode SKIP obj %u(0x%x) parent"
                        " 0x%x\n", (unsigned int) handle,
                        (unsigned int) handle,
                        (unsigned int) (size_t) parent);
                javacall_diag_log(line);
            }
            KNI_ReturnInt(M3G_PSP_RENDER_OK);
        }
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(array);
    KNI_GetParameterAsObject(2, array);
    transform = m3gFetchTransform(array, matrix);
    KNI_EndHandles();

    m3gEvent("renderNode>", handle, 0);
    result = m3gPspRenderNode((M3GObject) handle, transform);
    m3gEvent("renderNode<", handle, result);

    m3gMilestone((result == M3G_PSP_RENDER_OK) ? M3G_MILESTONE_RENDER_OK
                                               : M3G_MILESTONE_RENDER_FAIL,
                 (result == M3G_PSP_RENDER_OK) ? "renderNode ok"
                                               : "renderNode failed",
                 handle, result);
    m3gTrace("renderNode", handle, result);
    KNI_ReturnInt(result);
}

/*
 * private static native int nRenderImmediate(int vertices, int triangles,
 *                                            int appearance, float[] transform,
 *                                            int scope);
 *
 * Immediate-mode submission. It works for geometry that came out of Loader,
 * because those wrappers carry engine handles; geometry a MIDlet builds with
 * the public constructors does not have one yet, and is traced rather than
 * silently dropped -- a blank viewport with nothing but these lines in the log
 * says the title builds its meshes in Java, which is a different piece of work
 * from making the renderer draw.
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Graphics3D_nRenderImmediate()
{
    jint vertices   = KNI_GetParameterAsInt(1);
    jint triangles  = KNI_GetParameterAsInt(2);
    jint appearance = KNI_GetParameterAsInt(3);
    jint scope      = KNI_GetParameterAsInt(5);
    M3Gfloat matrix[M3G_MATRIX_FLOATS];
    const M3Gfloat *transform;
    jint result;

    if (vertices == 0 || triangles == 0) {
        m3gTrace("renderImmediate nohandle", vertices, triangles);
        KNI_ReturnInt(M3G_PSP_ERR_INVALID);
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(array);
    KNI_GetParameterAsObject(4, array);
    transform = m3gFetchTransform(array, matrix);
    KNI_EndHandles();

    result = m3gPspRenderImmediate((M3GObject) vertices,
                                   (M3GObject) triangles,
                                   (M3GObject) appearance,
                                   transform, scope);
    m3gTrace("renderImmediate", vertices, result);
    KNI_ReturnInt(result);
}

/*
 * private static native int nSetCamera(int cameraHandle, float[] transform);
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Graphics3D_nSetCamera()
{
    jint handle = KNI_GetParameterAsInt(1);
    M3Gfloat matrix[M3G_MATRIX_FLOATS];
    const M3Gfloat *transform;
    jint result;

    KNI_StartHandles(1);
    KNI_DeclareHandle(array);
    KNI_GetParameterAsObject(2, array);
    transform = m3gFetchTransform(array, matrix);
    KNI_EndHandles();

    result = m3gPspSetCamera((M3GObject) handle, transform);
    if (handle != 0) {
        m3gMilestone(M3G_MILESTONE_CAMERA, "setCamera", handle, result);
    }
    m3gTrace("setCamera", handle, result);
    KNI_ReturnInt(result);
}

/*
 * private static native int nAddLight(int lightHandle, float[] transform);
 *
 * Returns the light index (>= 0) or a negative M3G_PSP_ERR_*.
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Graphics3D_nAddLight()
{
    jint handle = KNI_GetParameterAsInt(1);
    M3Gfloat matrix[M3G_MATRIX_FLOATS];
    const M3Gfloat *transform;

    if (handle == 0) {
        KNI_ReturnInt(M3G_PSP_ERR_INVALID);
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(array);
    KNI_GetParameterAsObject(2, array);
    transform = m3gFetchTransform(array, matrix);
    KNI_EndHandles();

    KNI_ReturnInt(m3gPspAddLight((M3GObject) handle, transform));
}

/*
 * private static native void nClearLights();
 */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Graphics3D_nClearLights()
{
    m3gPspClearLights();
    KNI_ReturnVoid();
}
