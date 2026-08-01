/*
 * m3g_egl_stub.c -- no-op EGL backend.
 *
 * Counterpart to m3g_gl_stub.c: satisfies every EGL entry point that
 * src/m3g_rendercontext.inl and src/m3g_interface.c reference so the PSP ELF
 * links.  Nothing is rendered and no PSP resource is touched.
 *
 * Handles are returned as small non-NULL constants rather than NULL because
 * m3gcore checks them:
 *
 *   src/m3g_rendercontext.inl:168  returns the config only if numConfigs > 0
 *   src/m3g_rendercontext.inl:222  bails out if eglGetConfigAttrib() is false
 *   src/m3g_rendercontext.inl:1488 requires eglQuerySurface() for EGL_WIDTH
 *                                  and EGL_HEIGHT to both succeed
 *   src/m3g_interface.c:1257-1284  chooses a config, creates a context and a
 *                                  2x2 pbuffer, and makes it current during
 *                                  m3gCreateInterface()
 *
 * Returning failure from any of those would make the engine take an error path
 * during construction, which is not what "not implemented yet" should look
 * like: the intent is that the object graph works and only drawing is absent.
 *
 * eglGetError() always reports EGL_SUCCESS -- src/m3g_rendercontext.inl:156
 * treats anything else as fatal.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include <EGL/egl.h>

/* Arbitrary non-NULL handles. They are never dereferenced by anything, here or
 * in m3gcore -- EGLDisplay/EGLConfig/EGLContext/EGLSurface are opaque. */
#define STUB_DISPLAY    ((EGLDisplay) 1)
#define STUB_CONFIG     ((EGLConfig)  1)
#define STUB_CONTEXT    ((EGLContext) 1)
#define STUB_SURFACE    ((EGLSurface) 1)

/* Reported surface size when the engine asks. The PSP framebuffer. */
#define STUB_SURFACE_W  480
#define STUB_SURFACE_H  272

EGLint eglGetError (void)
{
    return EGL_SUCCESS;
}

EGLDisplay eglGetDisplay (EGLNativeDisplayType display_id)
{
    (void) display_id;
    return STUB_DISPLAY;
}

EGLDisplay eglGetCurrentDisplay (void)
{
    return STUB_DISPLAY;
}

EGLBoolean eglInitialize (EGLDisplay dpy, EGLint *major, EGLint *minor)
{
    (void) dpy;
    if (major != 0) { *major = 1; }
    if (minor != 0) { *minor = 1; }
    return EGL_TRUE;
}

EGLBoolean eglTerminate (EGLDisplay dpy)
{
    (void) dpy;
    return EGL_TRUE;
}

EGLBoolean eglBindAPI (EGLenum api)
{
    (void) api;
    return EGL_TRUE;
}

const char *eglQueryString (EGLDisplay dpy, EGLint name)
{
    (void) dpy;
    switch (name) {
    case EGL_VENDOR:      return "PSPKVM";
    case EGL_VERSION:     return "1.1 PSPKVM M3G null backend";
    case EGL_EXTENSIONS:  return "";
    case EGL_CLIENT_APIS: return "OpenGL_ES";
    default:              return "";
    }
}

EGLBoolean eglChooseConfig (EGLDisplay dpy, const EGLint *attrib_list,
                            EGLConfig *configs, EGLint config_size,
                            EGLint *num_config)
{
    (void) dpy; (void) attrib_list;

    /* Always report exactly one matching config, so the engine stops on its
     * first (highest quality) attempt instead of walking down the
     * multisampling ladder. */
    if (configs != 0 && config_size > 0) {
        configs[0] = STUB_CONFIG;
    }
    if (num_config != 0) {
        *num_config = (config_size > 0) ? 1 : 0;
    }
    return EGL_TRUE;
}

EGLBoolean eglGetConfigAttrib (EGLDisplay dpy, EGLConfig config,
                               EGLint attribute, EGLint *value)
{
    (void) dpy; (void) config;
    if (value == 0) {
        return EGL_FALSE;
    }
    switch (attribute) {
    case EGL_SURFACE_TYPE:
        /* Claim all three so no target kind is rejected up front. */
        *value = EGL_PBUFFER_BIT | EGL_PIXMAP_BIT | EGL_WINDOW_BIT;
        break;
    case EGL_RED_SIZE:
    case EGL_GREEN_SIZE:
    case EGL_BLUE_SIZE:
        *value = 8;
        break;
    case EGL_ALPHA_SIZE:
        *value = 8;
        break;
    case EGL_DEPTH_SIZE:
        *value = 16;
        break;
    case EGL_STENCIL_SIZE:
    case EGL_SAMPLES:
    case EGL_SAMPLE_BUFFERS:
        *value = 0;
        break;
    case EGL_CONFIG_ID:
        *value = 1;
        break;
    default:
        *value = 0;
        break;
    }
    return EGL_TRUE;
}

EGLContext eglCreateContext (EGLDisplay dpy, EGLConfig config,
                             EGLContext share_context, const EGLint *attrib_list)
{
    (void) dpy; (void) config; (void) share_context; (void) attrib_list;
    return STUB_CONTEXT;
}

EGLBoolean eglDestroyContext (EGLDisplay dpy, EGLContext ctx)
{
    (void) dpy; (void) ctx;
    return EGL_TRUE;
}

EGLBoolean eglQueryContext (EGLDisplay dpy, EGLContext ctx,
                            EGLint attribute, EGLint *value)
{
    (void) dpy; (void) ctx;
    if (value == 0) {
        return EGL_FALSE;
    }
    switch (attribute) {
    case EGL_CONFIG_ID: *value = 1; break;
    default:            *value = 0; break;
    }
    return EGL_TRUE;
}

EGLSurface eglCreateWindowSurface (EGLDisplay dpy, EGLConfig config,
                                   EGLNativeWindowType win,
                                   const EGLint *attrib_list)
{
    (void) dpy; (void) config; (void) win; (void) attrib_list;
    return STUB_SURFACE;
}

EGLSurface eglCreatePixmapSurface (EGLDisplay dpy, EGLConfig config,
                                   EGLNativePixmapType pixmap,
                                   const EGLint *attrib_list)
{
    (void) dpy; (void) config; (void) pixmap; (void) attrib_list;
    return STUB_SURFACE;
}

EGLSurface eglCreatePbufferSurface (EGLDisplay dpy, EGLConfig config,
                                    const EGLint *attrib_list)
{
    (void) dpy; (void) config; (void) attrib_list;
    return STUB_SURFACE;
}

EGLBoolean eglDestroySurface (EGLDisplay dpy, EGLSurface surface)
{
    (void) dpy; (void) surface;
    return EGL_TRUE;
}

EGLBoolean eglQuerySurface (EGLDisplay dpy, EGLSurface surface,
                            EGLint attribute, EGLint *value)
{
    (void) dpy; (void) surface;
    if (value == 0) {
        return EGL_FALSE;
    }
    switch (attribute) {
    case EGL_WIDTH:     *value = STUB_SURFACE_W; break;
    case EGL_HEIGHT:    *value = STUB_SURFACE_H; break;
    case EGL_CONFIG_ID: *value = 1;              break;
    default:            *value = 0;              break;
    }
    return EGL_TRUE;
}

EGLBoolean eglMakeCurrent (EGLDisplay dpy, EGLSurface draw, EGLSurface read,
                           EGLContext ctx)
{
    (void) dpy; (void) draw; (void) read; (void) ctx;
    return EGL_TRUE;
}

EGLBoolean eglSwapBuffers (EGLDisplay dpy, EGLSurface surface)
{
    (void) dpy; (void) surface;
    return EGL_TRUE;
}

EGLBoolean eglCopyBuffers (EGLDisplay dpy, EGLSurface surface,
                           EGLNativePixmapType target)
{
    (void) dpy; (void) surface; (void) target;
    return EGL_TRUE;
}

EGLBoolean eglWaitNative (EGLint engine)
{
    (void) engine;
    return EGL_TRUE;
}
