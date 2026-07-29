/*
 * m3g_psp_render.c -- the rendering half of the PSP port.
 *
 * m3gcore renders through EGL and GL ES 1.x only (the software path is
 * #error'd out at src/m3g_rendercontext.inl:25), and on this platform that
 * means pspgl.  Everything that has exactly one correct answer about how the
 * engine is driven lives here, so the KNI layer in jsr184/src/native stays
 * pure marshalling.  See inc/M3G/m3g_psp.h for the contract.
 *
 * The target is a *memory* target, not a window.
 *
 * PSPKVM already owns the screen: it drives the GE itself through sceGu and
 * blits the MIDP 16-bit screen buffer as a textured quad on every flush
 * (javacall/implementation/psp_mips/midp/lcd.c:121-138), ending in
 * sceGuSwapBuffers().  Rendering 3D straight to a pspgl window surface would
 * put the two in a fight over the scanout and make composition impossible --
 * every M3G frame would overwrite the MIDP UI and vice versa.
 *
 * m3gBindMemoryTarget avoids that entirely.  m3gcore cannot render directly
 * into a memory target, so it renders into an offscreen pbuffer and reads the
 * result back with glReadPixels, converting to the target's pixel format on
 * the way (src/m3g_rendercontext.inl:1013-1105).  Point that at MIDP's screen
 * buffer and the 3D lands *under* whatever 2D the MIDlet draws afterwards,
 * with PSPKVM's existing blit putting the composite on screen unchanged.
 *
 * Two details make it work:
 *
 * 1. Buffered rendering has to be forced.  m3gValidateBuffers decides between
 *    direct and buffered by asking EGL whether the target format is directly
 *    renderable (m3gCanDirectRender, :746), and pspgl's eglChooseConfig
 *    ignores EGL_SURFACE_TYPE -- so it would answer yes for a memory target,
 *    for which m3gSelectGLSurface has no case at all (:1296, default:
 *    assert-and-fail).  M3G_FORCE_BUFFERED_RENDERING (:583) is the engine's
 *    own switch for exactly this and is set in m3g/Makefile.
 *
 * 2. The back buffer is seeded from the target unless the OVERWRITE hint is
 *    set, so 2D drawn *before* the 3D survives (:952-1000).  That is the
 *    default and it is left alone.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include "M3G/m3g_psp.h"

#include <stddef.h>     /* NULL */

/*----------------------------------------------------------------------
 * The context singleton
 *
 * One per interface, and there is one interface for the whole VM
 * (m3g_psp_loader.c), which matches JSR-184: Graphics3D is a singleton.  It
 * is also what pspgl requires -- libGL.a has a single global context
 * (__pspgl_curctx) and not one lock anywhere in the archive, so every GL call
 * has to come from one place.
 *--------------------------------------------------------------------*/

static M3GRenderContext s_context = NULL;
static M3Gbool          s_bound   = M3G_FALSE;

/*!
 * \brief The context, brought up on first use.
 *
 * THIS IS WHERE THE RENDERER STARTS.  m3gPspGetInterface below reserves the
 * video memory and runs m3gCreateInterface, which brings EGL and pspgl up (see
 * the note in inc/M3G/m3g_psp.h).  Its only caller is m3gPspBindMemoryTarget,
 * i.e. Graphics3D.bindTarget, which is a point the MIDlet has chosen to draw
 * at -- not somewhere in the middle of PSPKVM painting its own 2D.  Nothing
 * that merely constructs an M3G object may reach this.
 */
M3GRenderContext m3gPspGetContext(void)
{
    if (s_context == NULL) {
        M3GInterface m3g = m3gPspGetInterface();
        if (m3g == NULL) {
            return NULL;
        }
        s_context = m3gCreateContext(m3g);
    }
    return s_context;
}

/*----------------------------------------------------------------------
 * Error mapping
 *--------------------------------------------------------------------*/

static M3Gint m3gPspRenderError(M3GInterface m3g)
{
    M3Genum error;

    /* The callers all run behind a check that a context exists, which implies
     * an interface -- but they now ask for it with m3gPspPeekInterface, whose
     * whole point is that it may answer NULL, and m3gGetError dereferences its
     * argument without checking. */
    if (m3g == NULL) {
        return M3G_PSP_ERR_NO_INTERFACE;
    }
    error = m3gGetError(m3g);

    switch (error) {
    case M3G_NO_ERROR:          return M3G_PSP_RENDER_OK;
    case M3G_OUT_OF_MEMORY:     return M3G_PSP_ERR_OUT_OF_MEMORY;
    case M3G_INVALID_VALUE:     /* fall through */
    case M3G_INVALID_ENUM:      return M3G_PSP_ERR_INVALID;
    default:                    return M3G_PSP_ERR_RENDER;
    }
}

M3Gint m3gPspTakeError(void)
{
    M3GInterface m3g = m3gPspPeekInterface();
    return (m3g != NULL) ? m3gPspRenderError(m3g) : M3G_PSP_ERR_NO_INTERFACE;
}

/*----------------------------------------------------------------------
 * Target binding
 *--------------------------------------------------------------------*/

M3Gint m3gPspBindMemoryTarget(void *pixels,
                              M3Gint width, M3Gint height,
                              M3Gint strideBytes,
                              M3Gint depthBuffer,
                              M3Gint hints)
{
    M3GRenderContext ctx;
    M3Gbitmask bufferBits;

    if (pixels == NULL || width <= 0 || height <= 0
        || strideBytes < width * 2) {
        return M3G_PSP_ERR_INVALID;
    }
    if (s_bound) {
        /* m3gBindRenderTarget would raise M3G_INVALID_OPERATION; say so up
         * front instead, because the Java layer turns this into the
         * IllegalStateException the specification asks for. */
        return M3G_PSP_ERR_BOUND;
    }

    ctx = m3gPspGetContext();
    if (ctx == NULL) {
        return M3G_PSP_ERR_NO_INTERFACE;
    }

    /* Clear anything a previous operation left behind so what is read back
     * below belongs to this bind. */
    m3gGetError(m3gPspPeekInterface());

    bufferBits = (M3Gbitmask) M3G_COLOR_BUFFER_BIT;
    if (depthBuffer) {
        bufferBits |= (M3Gbitmask) M3G_DEPTH_BUFFER_BIT;
    }
    m3gSetRenderBuffers(ctx, bufferBits);

    /* The JSR-184 hint bits and the engine's mode bits are the same values
     * (M3G/m3g_core.h:433-436 against the constants in Graphics3D), so they
     * pass straight through.  A rejected hint is not an error: the
     * specification allows an implementation to ignore any of them. */
    m3gSetRenderHints(ctx, (M3Gbitmask) hints);

    /* userHandle 0: it is only used to key the GL surface cache and the
     * invalidate calls, neither of which is reachable on the buffered path,
     * and a non-zero value makes m3gQueryEGLConfig add EGL_MATCH_NATIVE_PIXMAP
     * to the attribute list -- which pspgl's eglChooseConfig rejects outright,
     * leaving m3gcore to read an uninitialised config count. */
    m3gBindMemoryTarget(ctx,
                        pixels,
                        (M3Guint) width, (M3Guint) height,
                        M3G_RGB565,
                        (M3Guint) strideBytes,
                        0);

    {
        M3Gint err = m3gPspRenderError(m3gPspPeekInterface());
        if (err != M3G_PSP_RENDER_OK) {
            return err;
        }
    }

    s_bound = M3G_TRUE;
    return M3G_PSP_RENDER_OK;
}

M3Gint m3gPspReleaseTarget(void)
{
    M3GRenderContext ctx = s_context;

    if (ctx == NULL || !s_bound) {
        return M3G_PSP_RENDER_OK;
    }

    /* This is where the picture actually appears: the pbuffer is read back
     * with glReadPixels and converted into the caller's 16-bit buffer. */
    m3gReleaseTarget(ctx);
    s_bound = M3G_FALSE;

    return m3gPspRenderError(m3gPspPeekInterface());
}

M3Gint m3gPspIsBound(void)
{
    return s_bound ? 1 : 0;
}

/*----------------------------------------------------------------------
 * Frame state
 *--------------------------------------------------------------------*/

void m3gPspSetViewport(M3Gint x, M3Gint y, M3Gint width, M3Gint height)
{
    if (s_context != NULL) {
        m3gSetViewport(s_context, x, y, width, height);
    }
}

void m3gPspSetClipRect(M3Gint x, M3Gint y, M3Gint width, M3Gint height)
{
    if (s_context != NULL) {
        m3gSetClipRect(s_context, x, y, width, height);
    }
}

void m3gPspSetDepthRange(M3Gfloat depthNear, M3Gfloat depthFar)
{
    if (s_context != NULL) {
        m3gSetDepthRange(s_context, depthNear, depthFar);
    }
}

/*----------------------------------------------------------------------
 * Drawing
 *--------------------------------------------------------------------*/

M3Gint m3gPspClear(M3GObject background)
{
    if (s_context == NULL || !s_bound) {
        return M3G_PSP_ERR_NOT_BOUND;
    }
    m3gClear(s_context, (M3GBackground) background);
    return m3gPspRenderError(m3gPspPeekInterface());
}

M3Gint m3gPspRenderWorld(M3GObject world)
{
    if (s_context == NULL || !s_bound) {
        return M3G_PSP_ERR_NOT_BOUND;
    }
    if (world == NULL) {
        return M3G_PSP_ERR_INVALID;
    }
    m3gRenderWorld(s_context, (M3GWorld) world);
    return m3gPspRenderError(m3gPspPeekInterface());
}

M3Gint m3gPspRenderNode(M3GObject node, const M3Gfloat *transform)
{
    M3GMatrix matrix;

    if (s_context == NULL || !s_bound) {
        return M3G_PSP_ERR_NOT_BOUND;
    }
    if (node == NULL) {
        return M3G_PSP_ERR_INVALID;
    }

    if (transform != NULL) {
        /* JSR-184 hands matrices out in row-major order (Transform.get), which
         * is the order m3gSetMatrixRows expects. */
        m3gSetMatrixRows(&matrix, transform);
    }
    m3gRenderNode(s_context, (M3GNode) node, (transform != NULL) ? &matrix : NULL);
    return m3gPspRenderError(m3gPspPeekInterface());
}

M3Gint m3gPspRenderImmediate(M3GObject vertices,
                             M3GObject triangles,
                             M3GObject appearance,
                             const M3Gfloat *transform,
                             M3Gint scope)
{
    M3GMatrix matrix;

    if (s_context == NULL || !s_bound) {
        return M3G_PSP_ERR_NOT_BOUND;
    }
    if (vertices == NULL || triangles == NULL) {
        return M3G_PSP_ERR_INVALID;
    }
    if (transform != NULL) {
        m3gSetMatrixRows(&matrix, transform);
    }
    /* alphaFactor 1.0: the Java API has no per-submission alpha in immediate
     * mode; the node alpha factor only applies to the retained-mode paths. */
    m3gRender(s_context,
              (M3GVertexBuffer) vertices,
              (M3GIndexBuffer) triangles,
              (M3GAppearance) appearance,
              (transform != NULL) ? &matrix : NULL,
              1.0f,
              scope);
    return m3gPspRenderError(m3gPspPeekInterface());
}

M3Gint m3gPspSetCamera(M3GObject camera, const M3Gfloat *transform)
{
    M3GMatrix matrix;

    if (s_context == NULL) {
        return M3G_PSP_ERR_NO_INTERFACE;
    }
    if (transform != NULL) {
        m3gSetMatrixRows(&matrix, transform);
    }
    m3gSetCamera(s_context, (M3GCamera) camera,
                 (transform != NULL) ? &matrix : NULL);
    return m3gPspRenderError(m3gPspPeekInterface());
}

M3Gint m3gPspAddLight(M3GObject light, const M3Gfloat *transform)
{
    M3GMatrix matrix;
    M3Gint index;

    if (s_context == NULL) {
        return M3G_PSP_ERR_NO_INTERFACE;
    }
    if (light == NULL) {
        return M3G_PSP_ERR_INVALID;
    }
    if (transform != NULL) {
        m3gSetMatrixRows(&matrix, transform);
    }
    index = m3gAddLight(s_context, (M3GLight) light,
                        (transform != NULL) ? &matrix : NULL);
    if (index < 0) {
        return m3gPspRenderError(m3gPspPeekInterface());
    }
    return index;
}

void m3gPspClearLights(void)
{
    if (s_context != NULL) {
        m3gClearLights(s_context);
    }
}
