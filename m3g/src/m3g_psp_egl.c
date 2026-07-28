/*
 * m3g_psp_egl.c -- the EGL entry points pspgl does not implement, plus the one
 * it implements too narrowly.
 *
 * pspgl's EGL is EGL 1.0 and deliberately minimal: it has exactly one display,
 * ignores most of the specification's bookkeeping, and keeps everything else in
 * file statics.  m3gcore is written against EGL 1.1 and calls six entry points
 * that simply are not in libGL.a (verified with psp-nm):
 *
 *   eglBindAPI eglQuerySurface eglQueryContext eglGetCurrentDisplay
 *   eglCopyBuffers eglCreatePixmapSurface
 *
 * and one, eglGetConfigAttrib, that pspgl does define but which answers only
 * EGL_WIDTH and EGL_HEIGHT and returns EGL_FALSE *without writing through the
 * pointer* for anything else.  That last part matters: m3gcore reads the value
 * back regardless (src/m3g_rendercontext.inl:1421-1424), so leaving pspgl's
 * version in place means deciding a render flag from an unwritten stack slot.
 * It is overridden here.  That is safe because every EGL function in libGL.a
 * lives in its own archive member and nothing inside the archive references
 * eglGetConfigAttrib, so pspgl's member is simply never pulled in.
 *
 * Two things make the missing six easy rather than hard:
 *
 *   1. pspgl's eglCreateContext ignores its EGLConfig argument entirely -- it
 *      allocates a context, memsets it and never looks at the config (confirmed
 *      by disassembly: the argument register is overwritten before first use).
 *      m3gcore's only use of eglQuerySurface(EGL_CONFIG_ID) is to feed the
 *      result straight back into eglCreateContext
 *      (src/m3g_rendercontext.inl:1179-1183), so the id need only be a stable
 *      token, not a real config.
 *
 *   2. m3gcore only ever asks eglQuerySurface for EGL_WIDTH/EGL_HEIGHT on an
 *      *externally supplied* EGL surface, in m3gBindEGLSurfaceTarget
 *      (:1488) -- a path this port does not use, because the target is bound
 *      with m3gBindMemoryTarget instead.  Pbuffer dimensions are therefore
 *      never queried, which is the one thing that could not be answered
 *      honestly from outside the library (pspgl keeps them in struct
 *      pspgl_surface, which is private).  The PSP display size is reported
 *      instead, which is what a window surface would answer anyway.
 *
 * This translation unit compiles against m3g/inc/EGL/egl.h, the same header
 * m3gcore sees.  pspgl's own EGL header lives at <GLES/egl.h> and declares the
 * same functions with cosmetically different typedefs (EGLConfig int vs void*,
 * EGLBoolean int vs unsigned); every one of them is 32 bits, so the o32 calling
 * convention is identical and the two must simply never meet in one file.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include <EGL/egl.h>

/* The PSP display is fixed at 480x272. */
#define PSP_SCREEN_W 480
#define PSP_SCREEN_H 272

/*!
 * \brief The token handed out for EGL_CONFIG_ID.
 *
 * pspgl encodes a config as (has_depth << 4) | pixel_config_index, so 0 is a
 * legal config (the first pixel format, no depth buffer).  It is never
 * dereferenced or matched against anything in this port -- see the note on
 * eglCreateContext above.
 */
#define PSP_CONFIG_ID 0

EGLBoolean eglBindAPI (EGLenum api)
{
    /* pspgl is GL ES only, and that is the only API m3gcore binds. */
    return (api == EGL_OPENGL_ES_API) ? EGL_TRUE : EGL_FALSE;
}

EGLDisplay eglGetCurrentDisplay (void)
{
    /* pspgl has exactly one display and eglGetDisplay ignores its argument. */
    return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

EGLBoolean eglQuerySurface (EGLDisplay dpy, EGLSurface surface,
                            EGLint attribute, EGLint *value)
{
    (void) dpy;
    if (surface == EGL_NO_SURFACE || value == 0) {
        return EGL_FALSE;
    }

    switch (attribute) {
    case EGL_CONFIG_ID:
        *value = PSP_CONFIG_ID;
        return EGL_TRUE;
    case EGL_WIDTH:
        *value = PSP_SCREEN_W;
        return EGL_TRUE;
    case EGL_HEIGHT:
        *value = PSP_SCREEN_H;
        return EGL_TRUE;
    case EGL_LARGEST_PBUFFER:
        *value = EGL_FALSE;
        return EGL_TRUE;
    default:
        return EGL_FALSE;
    }
}

EGLBoolean eglQueryContext (EGLDisplay dpy, EGLContext ctx,
                            EGLint attribute, EGLint *value)
{
    (void) dpy;
    if (ctx == EGL_NO_CONTEXT || value == 0) {
        return EGL_FALSE;
    }

    switch (attribute) {
    case EGL_CONFIG_ID:
        *value = PSP_CONFIG_ID;
        return EGL_TRUE;
    default:
        return EGL_FALSE;
    }
}

EGLBoolean eglGetConfigAttrib (EGLDisplay dpy, EGLConfig config,
                               EGLint attribute, EGLint *value)
{
    (void) dpy;
    if (value == 0) {
        return EGL_FALSE;
    }

    switch (attribute) {
    case EGL_CONFIG_CAVEAT:
        /* No caveat: the GE is the only rasteriser there is, so every config
         * pspgl offers is the hardware one.  m3gcore turns this into
         * ctx->accelerated, which selects linear rather than nearest filtering
         * for backgrounds and sprites (src/m3g_background.c:191,
         * src/m3g_sprite.c:459). */
        *value = EGL_NONE;
        return EGL_TRUE;
    case EGL_CONFIG_ID:
        *value = (EGLint) (long) config;
        return EGL_TRUE;
    case EGL_SURFACE_TYPE:
        /* Window and pbuffer are real; pixmap is claimed so that no target
         * kind is rejected up front by m3gQueryEGLConfig.  The pixmap path is
         * refused later, at eglCreatePixmapSurface. */
        *value = EGL_PBUFFER_BIT | EGL_PIXMAP_BIT | EGL_WINDOW_BIT;
        return EGL_TRUE;
    case EGL_WIDTH:
        *value = PSP_SCREEN_W;
        return EGL_TRUE;
    case EGL_HEIGHT:
        *value = PSP_SCREEN_H;
        return EGL_TRUE;
    default:
        return EGL_FALSE;
    }
}

EGLBoolean eglCopyBuffers (EGLDisplay dpy, EGLSurface surface,
                           EGLNativePixmapType target)
{
    /* No native pixmaps on the PSP.  m3gcore treats a false return as "the
     * fast path is unavailable" and falls back to glReadPixels
     * (src/m3g_rendercontext.inl:1041-1055), which is the path this port
     * uses for every target anyway. */
    (void) dpy; (void) surface; (void) target;
    return EGL_FALSE;
}

EGLSurface eglCreatePixmapSurface (EGLDisplay dpy, EGLConfig config,
                                   EGLNativePixmapType pixmap,
                                   const EGLint *attrib_list)
{
    /* Ditto: there is no native bitmap handle space on the PSP, so the
     * SURFACE_BITMAP path is refused outright.  m3g_psp_native.c refuses the
     * matching m3gglGetNativeBitmapParams for the same reason. */
    (void) dpy; (void) config; (void) pixmap; (void) attrib_list;
    return EGL_NO_SURFACE;
}
