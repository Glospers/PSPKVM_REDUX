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
 * \brief Set once the first render target has been bound successfully.
 *
 * Not the same question as s_bound, which is "is one bound right now".  This
 * one gates building engine objects at all: creating an Image2D commits it and
 * commits upload the texture immediately, through GL
 * (m3gcore/src/m3g_image.inl:146, :157, :190).  Doing that before a target has
 * ever been bound means driving the GE from whatever point in its startup the
 * MIDlet happens to construct a texture -- while PSPKVM is still painting its
 * own 2D with sceGu, which is the one thing this port cannot do.
 */
static M3Gbool          s_everBound = M3G_FALSE;

/* The bound target's size, for the read-back hint at release. */
static M3Gint           s_targetWidth;
static M3Gint           s_targetHeight;

M3Gint m3gPspRendererReady(void)
{
    return s_everBound ? 1 : 0;
}

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
        || strideBytes < width * 4) {
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
     * specification allows an implementation to ignore any of them.
     *
     * OVERWRITE must NOT be forced on here.  Doing that (tried 2026-08-02)
     * stops the engine blitting the target's prior content into the back
     * buffer, which saves a full-frame upload per frame -- and breaks this
     * title outright: it paints its warm radiation gradient as 2D bands
     * BEFORE bindTarget and expects the 3D to composite over them, so the
     * hue vanished, and without that opaque backdrop covering the frame the
     * motion trails came back.  2D-under-3D is real and load-bearing. */
    m3gSetRenderHints(ctx, (M3Gbitmask) hints);

    /* userHandle 0: it is only used to key the GL surface cache and the
     * invalidate calls, neither of which is reachable on the buffered path,
     * and a non-zero value makes m3gQueryEGLConfig add EGL_MATCH_NATIVE_PIXMAP
     * to the attribute list -- which pspgl's eglChooseConfig rejects outright,
     * leaving m3gcore to read an uninitialised config count. */
    /* RGBA8, not the RGB565 the MIDP screen actually is.  Two reasons, both
     * discovered the hard way:
     *   - the engine's blit of the target's prior content INTO the frame
     *     (m3gUpdateBackBuffer, the path that makes 2D show through under a
     *      Background with colour clear disabled) silently drops formats
     *     m3gGetGLFormat does not know, and M3G_RGB565 is one of them
     *     (m3gcore/src/m3g_image.inl:78, no 565 case).  This title clears
     *     depth-only every frame and paints its sky as 2D underneath, so
     *     that blit is the whole ballgame -- and its absence also meant
     *     nothing ever erased the previous frame: the ghosting.
     *   - m3gConvertPixels packs 565 with red in the high bits; the PSP
     *     framebuffer wants red low, so every readback frame came out with
     *     red and blue exchanged.
     * The KNI layer converts 565<->RGBA8 at bind and release, where the
     * byte order is under our control. */
    m3gBindMemoryTarget(ctx,
                        pixels,
                        (M3Guint) width, (M3Guint) height,
                        M3G_RGBA8,
                        (M3Guint) strideBytes,
                        0);

    {
        M3Gint err = m3gPspRenderError(m3gPspPeekInterface());
        if (err != M3G_PSP_RENDER_OK) {
            return err;
        }
    }

    s_bound = M3G_TRUE;
    s_everBound = M3G_TRUE;
    s_targetWidth  = width;
    s_targetHeight = height;
    return M3G_PSP_RENDER_OK;
}

M3Gint m3gPspReleaseTarget(void)
{
    M3GRenderContext ctx = s_context;

    if (ctx == NULL || !s_bound) {
        return M3G_PSP_RENDER_OK;
    }

    /* This is where the picture actually appears: the pbuffer is read back
     * with glReadPixels and converted into the caller's 16-bit buffer.
     * Announce the frame first, so the read-back corrector can fetch it in
     * one native transfer instead of one GE sync per chunk -- see
     * m3gPspReadbackHint in src/m3g_psp_gl.c. */
    m3gPspReadbackHint(0, 0, s_targetWidth, s_targetHeight);
    m3gReleaseTarget(ctx);
    s_bound = M3G_FALSE;

    /* m3gcore made its own context current for the frame and does not put ours
     * back. Re-assert it, so that anything touching GL between frames -- an
     * Image2D being committed, a texture being freed -- still finds a current
     * context rather than pspgl's null one. */
    m3gPspHoldGLContext();

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

#if defined(M3G_PSP_SCENE_BACKDROP)
    /*
     * TEMPORARY -- a known backdrop, so geometry cannot hide.
     *
     * This title clears depth but deliberately not colour -- the phone-era
     * assumption that the scene repaints every pixel.  Its scene is not
     * repainting anything here, so the buffer shows its accumulated past
     * (white) instead, and whether triangles rasterise at all cannot be told
     * from looking at it.  A flat dark blue underneath makes the question
     * answerable: shapes over blue mean geometry draws and the fault is in
     * the scene (skybox, textures, lighting); flat blue means triangles are
     * being culled wholesale and the fault is depth or transforms.
     */
    {
        extern void glClearColor(float, float, float, float);
        extern void glClear(unsigned int);
        glClearColor(0.0f, 0.0f, 0.25f, 1.0f);
        glClear(0x4000 /* GL_COLOR_BUFFER_BIT */);
    }
#endif

#if defined(M3G_PSP_PROBE_TRIANGLE)
    /*
     * TEMPORARY -- can pspgl rasterise anything at all in this frame?
     *
     * The engine emits draw calls for these scenes on the host harness, and
     * on the device nothing appears; the difference is the real GL backend.
     * This draws one yellow triangle the way the proven spinning-cube demo
     * drew (float arrays, identity matrices, immediate state) into the same
     * frame the engine renders.  Yellow on screen: pspgl draws fine and the
     * fault is in what the engine asks of it.  No yellow: pspgl cannot draw
     * into this pbuffer at all, and the scene never had a chance.
     */
    {
        /* The control: float vertices, glLoadIdentity -- proven to draw. */
        static const float yellow[9] = {
            -0.8f, -0.8f, 0.0f,
             0.8f, -0.8f, 0.0f,
             0.0f,  0.8f, 0.0f,
        };
        /* The engine's vertex style: GL_SHORT arrays.  Shorts are integers,
         * so the corners of clip space are the only useful coordinates; this
         * is a thin wedge along the bottom edge. */
        static const short red[9] = {
            -1, -1, 0,
             1, -1, 0,
             0,  0, 0,
        };
        /* The engine's matrix style: an explicit column-major matrix through
         * glLoadMatrixf -- identity except a shift right and up, so it only
         * shows where the yellow control is not. */
        static const float greenMtx[16] = {
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.55f, 0.55f, 0.f, 1.f,
        };
        static const float green[9] = {
            -0.25f, -0.25f, 0.0f,
             0.25f, -0.25f, 0.0f,
             0.00f,  0.25f, 0.0f,
        };

        extern void glMatrixMode(unsigned int);
        extern void glLoadIdentity(void);
        extern void glLoadMatrixf(const float *);
        extern void glDisable(unsigned int);
        extern void glDisableClientState(unsigned int);
        extern void glEnableClientState(unsigned int);
        extern void glVertexPointer(int, unsigned int, int, const void *);
        extern void glDrawArrays(unsigned int, int, int);
        extern void glColor4f(float, float, float, float);

        glMatrixMode(0x1701 /* GL_PROJECTION */);
        glLoadIdentity();
        glMatrixMode(0x1700 /* GL_MODELVIEW */);
        glLoadIdentity();
        glDisable(0x0B71 /* GL_DEPTH_TEST */);
        glDisable(0x0B44 /* GL_CULL_FACE  */);
        glDisable(0x0DE1 /* GL_TEXTURE_2D */);
        glDisable(0x0B50 /* GL_LIGHTING   */);
        glDisableClientState(0x8078 /* GL_TEXTURE_COORD_ARRAY */);
        glDisableClientState(0x8076 /* GL_COLOR_ARRAY  */);
        glDisableClientState(0x8075 /* GL_NORMAL_ARRAY */);
        glEnableClientState(0x8074 /* GL_VERTEX_ARRAY */);

        /* Round 2 of the bisect.  Round 1 (2026-07-31) proved: GL_SHORT
         * vertex arrays draw (red), glLoadMatrixf honours its values (green,
         * correctly displaced), float control (yellow).  What remains
         * untested is exactly what the engine does and the probes did not:
         * indexed strips, multiple enabled arrays, and texturing. */

        /* MAGENTA, top-left -- glDrawElements + GL_TRIANGLE_STRIP + byte
         * indices: the engine's only draw shape (TriangleStripArray). */
        static const float quadTL[12] = {
            -0.9f,  0.4f, 0.0f,
            -0.4f,  0.4f, 0.0f,
            -0.9f,  0.9f, 0.0f,
            -0.4f,  0.9f, 0.0f,
        };
        static const unsigned char stripIdx[4] = { 0, 1, 2, 3 };

        /* CYAN, left-middle -- short verts with normal and texcoord arrays
         * ENABLED alongside (texturing off): the engine's vertex layout. */
        static const short cyanVerts[9]     = { -95, -30, 0, -55, -30, 0, -75, 10, 0 };
        static const signed char cyanNorms[9]  = { 0, 0, 127, 0, 0, 127, 0, 0, 127 };
        static const short cyanTex[6]       = { 0, 0, 1, 0, 0, 1 };
        static const float cyanMtx[16] = {
            0.01f, 0.f, 0.f, 0.f,
            0.f, 0.01f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };

        /* WHITE/RED checker, upper-right -- textured draw: 2x2 texture,
         * NEAREST filters (the default minification wants mipmaps and an
         * incomplete texture renders undefined -- a classic silent black). */
        static const float texQuad[12] = {
            0.3f,  0.3f, 0.0f,
            0.9f,  0.3f, 0.0f,
            0.3f,  0.9f, 0.0f,
            0.9f,  0.9f, 0.0f,
        };
        static const float texUV[8] = {
            0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 1.f,
        };
        /* 16x16, 8-pixel checker: a 2x2 texture sits below the GE's
         * practical minimum buffer width and samples unreliably -- the
         * solid-white square of the first round was probably this probe's
         * own artifact, not pspgl's. */
        static unsigned short texels[16 * 16];
        static int texelsReady;
        static unsigned int s_texName;

        extern void glGenTextures(int, unsigned int *);
        extern void glBindTexture(unsigned int, unsigned int);
        extern void glTexImage2D(unsigned int, int, int, int, int, int,
                                 unsigned int, unsigned int, const void *);
        extern void glTexParameteri(unsigned int, unsigned int, int);
        extern void glEnable(unsigned int);
        extern void glTexCoordPointer(int, unsigned int, int, const void *);
        extern void glNormalPointer(unsigned int, int, const void *);
        extern void glDrawElements(unsigned int, int, unsigned int, const void *);

        /* YELLOW -- the control, identical to the proven probe. */
        glColor4f(1.0f, 1.0f, 0.0f, 1.0f);
        glVertexPointer(3, 0x1406 /* GL_FLOAT */, 0, yellow);
        glDrawArrays(0x0004 /* GL_TRIANGLES */, 0, 3);

        /* MAGENTA -- indexed strip. */
        glColor4f(1.0f, 0.0f, 1.0f, 1.0f);
        glVertexPointer(3, 0x1406 /* GL_FLOAT */, 0, quadTL);
        glDrawElements(0x0005 /* GL_TRIANGLE_STRIP */, 4,
                       0x1401 /* GL_UNSIGNED_BYTE */, stripIdx);

        /* CYAN -- multi-array vertex layout, scaled shorts. */
        glLoadMatrixf(cyanMtx);
        glColor4f(0.0f, 1.0f, 1.0f, 1.0f);
        glEnableClientState(0x8075 /* GL_NORMAL_ARRAY */);
        glNormalPointer(0x1400 /* GL_BYTE */, 0, cyanNorms);
        glEnableClientState(0x8078 /* GL_TEXTURE_COORD_ARRAY */);
        glTexCoordPointer(2, 0x1402 /* GL_SHORT */, 0, cyanTex);
        glVertexPointer(3, 0x1402 /* GL_SHORT */, 0, cyanVerts);
        glDrawArrays(0x0004 /* GL_TRIANGLES */, 0, 3);
        glDisableClientState(0x8075);
        glDisableClientState(0x8078);
        glLoadIdentity();

        /* CHECKER -- textured strip. */
        if (!texelsReady) {
            int tx, ty;
            texelsReady = 1;
            for (ty = 0; ty < 16; ++ty) {
                for (tx = 0; tx < 16; ++tx) {
                    texels[ty * 16 + tx] =
                        (((tx >> 3) ^ (ty >> 3)) & 1) ? 0xF800 : 0xFFFF;
                }
            }
        }
        if (s_texName == 0) {
            glGenTextures(1, &s_texName);
            glBindTexture(0x0DE1 /* GL_TEXTURE_2D */, s_texName);
            glTexParameteri(0x0DE1, 0x2801 /* GL_TEXTURE_MIN_FILTER */,
                            0x2600 /* GL_NEAREST */);
            glTexParameteri(0x0DE1, 0x2800 /* GL_TEXTURE_MAG_FILTER */,
                            0x2600 /* GL_NEAREST */);
            glTexImage2D(0x0DE1, 0, 0x1907 /* GL_RGB */, 16, 16, 0,
                         0x1907, 0x8363 /* GL_UNSIGNED_SHORT_5_6_5 */, texels);
        }
        glEnable(0x0DE1 /* GL_TEXTURE_2D */);
        glBindTexture(0x0DE1, s_texName);
        /* pspgl samples through the GE's texture matrix (TEXMAPMODE 0x101),
         * whose content comes from a VFPU-managed stack no probe has ever
         * set.  Load it explicitly: if the checker appears now, that stack
         * held garbage and every textured draw in the engine dies the same
         * way. */
        glMatrixMode(0x1702 /* GL_TEXTURE */);
        glLoadIdentity();
        glMatrixMode(0x1700 /* GL_MODELVIEW */);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glEnableClientState(0x8078 /* GL_TEXTURE_COORD_ARRAY */);
        glTexCoordPointer(2, 0x1406 /* GL_FLOAT */, 0, texUV);
        glVertexPointer(3, 0x1406 /* GL_FLOAT */, 0, texQuad);
        glDrawElements(0x0005 /* GL_TRIANGLE_STRIP */, 4,
                       0x1401 /* GL_UNSIGNED_BYTE */, stripIdx);

        /* One-shot dump of pspgl's register shadow straight after the
         * textured draw: everything the GE debugger would have shown about
         * the texture unit, without the debugger.  ge_reg lives at offset 8
         * of the context (verified by disassembly of
         * __pspgl_context_writereg); pspgl_curctx is pspgl's exported
         * current-context global.  Registers 0x1E (texture enable),
         * 0xA0-0xAF (texture addresses/strides) and 0xB8-0xCF (size, mode,
         * format, filter, wrap, function, flush) cover the texture unit.
         */
        {
            extern void *__pspgl_curctx;
            extern void javacall_diag_log(const char *s)
                __attribute__((weak));
            static int dumped;

            if (!dumped && __pspgl_curctx != 0 && javacall_diag_log != 0) {
                const unsigned int *reg =
                    (const unsigned int *) ((char *) __pspgl_curctx + 8);
                /* 24 registers at 10 characters each plus the prefix: the
                 * first cut of this buffer was 200 bytes and the dump wrote
                 * 251, jumping the CPU into its own hex text. */
                char line[320];
                int r, n;

                dumped = 1;
                n = sprintf(line, "M3G: geTex ena=%08x", reg[0x1E]);
                for (r = 0xA0; r <= 0xAF; ++r) {
                    n += sprintf(line + n, " %02x=%06x", r, reg[r] & 0xFFFFFF);
                }
                line[n]     = '\n';
                line[n + 1] = '\0';
                javacall_diag_log(line);

                n = sprintf(line, "M3G: geTex2");
                for (r = 0xB8; r <= 0xCF; ++r) {
                    n += sprintf(line + n, " %02x=%06x", r, reg[r] & 0xFFFFFF);
                }
                line[n]     = '\n';
                line[n + 1] = '\0';
                javacall_diag_log(line);
            }
        }

        glDisableClientState(0x8078);
        glDisable(0x0DE1);

        /* Round 3. */

        /* WHITE, bottom-right -- glMultMatrixf: the one matrix operation the
         * engine uses per node that no probe has exercised.  pspgl multiplies
         * on the VFPU; garbage here means garbage model-view for every mesh. */
        {
            static const float shiftRight[16] = {
                1.f, 0.f, 0.f, 0.f,
                0.f, 1.f, 0.f, 0.f,
                0.f, 0.f, 1.f, 0.f,
                0.55f, 0.f, 0.f, 1.f,
            };
            static const float shiftDown[16] = {
                1.f, 0.f, 0.f, 0.f,
                0.f, 1.f, 0.f, 0.f,
                0.f, 0.f, 1.f, 0.f,
                0.f, -0.55f, 0.f, 1.f,
            };
            static const float small[9] = {
                -0.2f, -0.2f, 0.0f,
                 0.2f, -0.2f, 0.0f,
                 0.0f,  0.2f, 0.0f,
            };
            extern void glMultMatrixf(const float *);

            glLoadMatrixf(shiftRight);
            glMultMatrixf(shiftDown);   /* lands bottom-right iff mult works */
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            glVertexPointer(3, 0x1406 /* GL_FLOAT */, 0, small);
            glDrawArrays(0x0004, 0, 3);
            glLoadIdentity();
        }

        /* ORANGE, right edge -- depth-tested, as the engine draws: if the
         * depth buffer holds garbage the engine's fragments all fail the test
         * while every depth-off probe passes. Drawn at z=0 after the engine's
         * own depth clear. */
        {
            static const float edge[9] = {
                0.85f, -0.3f, 0.0f,
                1.00f, -0.3f, 0.0f,
                0.92f,  0.3f, 0.0f,
            };
            extern void glDepthFunc(unsigned int);

            glEnable(0x0B71 /* GL_DEPTH_TEST */);
            glDepthFunc(0x0203 /* GL_LEQUAL */);
            glColor4f(1.0f, 0.6f, 0.0f, 1.0f);
            glVertexPointer(3, 0x1406 /* GL_FLOAT */, 0, edge);
            glDrawArrays(0x0004, 0, 3);
            glDisable(0x0B71);
        }

        (void) red; (void) green; (void) greenMtx;
    }
#endif

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
