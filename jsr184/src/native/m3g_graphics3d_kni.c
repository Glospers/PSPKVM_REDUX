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
 * Diagnostics
 *
 * Same sink as the loader natives: ms0:/pspkvm_vm.log, weak so that a build
 * without docker/patches/0043 -- and the romgen host tool, which has no
 * javacall at all -- still links.
 *--------------------------------------------------------------------*/

extern void javacall_diag_log(const char *s) __attribute__((weak));

/* One line per run for the things that only make sense once. */
static int s_reportedOnce = 0;

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
 * Every log line is an open/write/close on the memory stick, so a trace of a
 * render loop has to be bounded or it becomes the thing being measured. This
 * budget is enough to see which entry points a title uses and in what order,
 * which is the question a blank viewport actually poses.
 */
#define M3G_TRACE_BUDGET 120
static int s_traceLeft = M3G_TRACE_BUDGET;

static void m3gTrace(const char *what, jint a, jint b)
{
    if (s_traceLeft <= 0) {
        return;
    }
    --s_traceLeft;
    m3gLog(what, a, b);
}

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

/*
 * private static native int nBind(int hints, int depthBuffer);
 *
 * Binds the MIDP screen buffer as the rendering target.  Returns the target
 * size packed as (width << 16) | height so that the Java side can set the
 * default viewport the specification asks for, or a negative M3G_PSP_ERR_*.
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Graphics3D_nBind()
{
    jint hints       = KNI_GetParameterAsInt(1);
    jint depthBuffer = KNI_GetParameterAsInt(2);

    unsigned short *pixels;
    int width = 0, height = 0, encoding = 0;
    jint result;

    pixels = javacall_lcd_get_screen(JAVACALL_LCD_SCREEN_PRIMARY,
                                     &width, &height, &encoding);

    if (pixels == NULL || width <= 0 || height <= 0
        || encoding != JAVACALL_LCD_COLOR_RGB565) {
        m3gLog("bind no-screen", width, height);
        KNI_ReturnInt(M3G_PSP_ERR_UNSUPPORTED);
    }

    result = m3gPspBindMemoryTarget(pixels, width, height,
                                    width * (M3Gint) sizeof(unsigned short),
                                    depthBuffer, hints);
    if (result != M3G_PSP_RENDER_OK) {
        m3gLog("bind failed", result, 0);
        KNI_ReturnInt(result);
    }

    if (!s_reportedOnce) {
        s_reportedOnce = 1;
        m3gLog("vram reserved", m3gPspGetReservedVram(), 0);
    }
    m3gTrace("bind ok", width, height);
    KNI_ReturnInt((width << 16) | height);
}

/*
 * private static native int nRelease();
 *
 * Reads the rendered frame back into the screen buffer and unbinds.
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Graphics3D_nRelease()
{
    jint result;

    /* Sample the target before and after the read-back. An unchanged marker
     * means glReadPixels never landed; a zeroed one means it landed and the
     * frame really is black. Only while the trace budget lasts, because this
     * writes into the live screen buffer. */
    if (s_traceLeft > 0) {
        unsigned short *pixels;
        int w = 0, h = 0, enc = 0;
        pixels = javacall_lcd_get_screen(JAVACALL_LCD_SCREEN_PRIMARY,
                                         &w, &h, &enc);
        if (pixels != NULL && w > 0 && h > 0) {
            pixels[(h / 2) * w + (w / 2)] = 0xF81F;   /* magenta marker */
            result = m3gPspReleaseTarget();
            m3gTrace("release px", result,
                     (jint) pixels[(h / 2) * w + (w / 2)]);
            KNI_ReturnInt(result);
        }
    }
    KNI_ReturnInt(m3gPspReleaseTarget());
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
    result = m3gPspRenderWorld((M3GObject) handle);
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

    KNI_StartHandles(1);
    KNI_DeclareHandle(array);
    KNI_GetParameterAsObject(2, array);
    transform = m3gFetchTransform(array, matrix);
    KNI_EndHandles();

    result = m3gPspRenderNode((M3GObject) handle, transform);
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
