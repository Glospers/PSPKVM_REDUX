/*
 * EGL/egl.h -- EGL 1.1 declarations for the M3G core library.
 *
 * m3gcore manages its rendering targets through EGL: inc/m3g_gl.h:32 includes
 * <EGL/egl.h>, and src/m3g_rendercontext.inl (the whole non-NGL context
 * backend) is written against it.  The PSP has no EGL, so this header declares
 * the subset m3gcore uses and the implementation is chosen at link time --
 * today that is the no-op backend in src/stub/m3g_egl_stub.c.
 *
 * Note that the pspdev toolchain does ship pspgl (libGL.a with headers in
 * <GLES/>), but its EGL lives in <GLES/egl.h>, is EGL 1.0 only, and is missing
 * eglBindAPI, eglQuerySurface, eglQueryContext, eglGetCurrentDisplay,
 * eglCopyBuffers and eglCreatePixmapSurface -- all of which m3gcore calls -- so
 * it cannot be used as a drop-in.  See the notes in GLES/gl.h.
 *
 * Names collected mechanically:
 *   grep -rhoE '\begl[A-Z][A-Za-z0-9]*\s*\(' m3gcore/src m3gcore/inc
 *   grep -rhoE '\bEGL_[A-Z0-9_]+'            m3gcore/src m3gcore/inc
 *
 * Token values are the standard EGL ones.
 *
 * This file is part of the PSPKVM JSR-184 work and is licensed under the
 * GNU General Public License version 2, like the rest of this repository.
 */

#ifndef __egl_h_
#define __egl_h_

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
 * Types
 *
 * The native handle types are plain integers here.  m3gcore only ever passes
 * M3GNativeBitmap / M3GNativeWindow (both M3Guint, M3G/m3g_core.h:175,:178)
 * through them, and on the PSP there is no window system object to point at.
 *--------------------------------------------------------------------*/

typedef int             EGLint;
typedef unsigned int    EGLBoolean;
typedef unsigned int    EGLenum;
typedef void           *EGLConfig;
typedef void           *EGLContext;
typedef void           *EGLDisplay;
typedef void           *EGLSurface;
typedef void           *EGLClientBuffer;

typedef int             EGLNativeDisplayType;
typedef int             EGLNativeWindowType;
typedef int             EGLNativePixmapType;

/* EGL 1.0 spellings, still used by some code */
typedef EGLNativeDisplayType NativeDisplayType;
typedef EGLNativeWindowType  NativeWindowType;
typedef EGLNativePixmapType  NativePixmapType;

/*----------------------------------------------------------------------
 * Tokens
 *--------------------------------------------------------------------*/

#define EGL_FALSE                       0
#define EGL_TRUE                        1

#define EGL_DEFAULT_DISPLAY             ((EGLNativeDisplayType)0)
#define EGL_NO_CONTEXT                  ((EGLContext)0)
#define EGL_NO_DISPLAY                  ((EGLDisplay)0)
#define EGL_NO_SURFACE                  ((EGLSurface)0)

/* Errors */
#define EGL_SUCCESS                     0x3000
#define EGL_NOT_INITIALIZED             0x3001
#define EGL_BAD_ACCESS                  0x3002
#define EGL_BAD_ALLOC                   0x3003
#define EGL_BAD_ATTRIBUTE               0x3004
#define EGL_BAD_CONFIG                  0x3005
#define EGL_BAD_CONTEXT                 0x3006
#define EGL_BAD_DISPLAY                 0x3008
#define EGL_BAD_MATCH                   0x3009
#define EGL_BAD_PARAMETER               0x300C
#define EGL_BAD_SURFACE                 0x300D

/* Config attributes */
#define EGL_BUFFER_SIZE                 0x3020
#define EGL_ALPHA_SIZE                  0x3021
#define EGL_BLUE_SIZE                   0x3022
#define EGL_GREEN_SIZE                  0x3023
#define EGL_RED_SIZE                    0x3024
#define EGL_DEPTH_SIZE                  0x3025
#define EGL_STENCIL_SIZE                0x3026
#define EGL_CONFIG_CAVEAT               0x3027
#define EGL_CONFIG_ID                   0x3028
#define EGL_LEVEL                       0x3029
#define EGL_NATIVE_RENDERABLE           0x302D
#define EGL_NATIVE_VISUAL_ID            0x302E
#define EGL_NATIVE_VISUAL_TYPE          0x302F
#define EGL_SAMPLES                     0x3031
#define EGL_SAMPLE_BUFFERS              0x3032
#define EGL_SURFACE_TYPE                0x3033
#define EGL_TRANSPARENT_TYPE            0x3034
#define EGL_NONE                        0x3038
#define EGL_MATCH_NATIVE_PIXMAP         0x3041

/* Surface type bits (EGL_SURFACE_TYPE) */
#define EGL_PBUFFER_BIT                 0x0001
#define EGL_PIXMAP_BIT                  0x0002
#define EGL_WINDOW_BIT                  0x0004

/* Surface / context attributes */
#define EGL_HEIGHT                      0x3056
#define EGL_WIDTH                       0x3057
#define EGL_LARGEST_PBUFFER             0x3058

/* eglQueryString targets */
#define EGL_VENDOR                      0x3053
#define EGL_VERSION                     0x3054
#define EGL_EXTENSIONS                  0x3055
#define EGL_CLIENT_APIS                 0x308D

/* eglWaitNative engines */
#define EGL_CORE_NATIVE_ENGINE          0x305B

/* Client APIs (eglBindAPI) */
#define EGL_OPENGL_ES_API               0x30A0
#define EGL_OPENVG_API                  0x30A1

/*----------------------------------------------------------------------
 * Entry points
 *--------------------------------------------------------------------*/

EGLBoolean  eglBindAPI (EGLenum api);
EGLBoolean  eglChooseConfig (EGLDisplay dpy, const EGLint *attrib_list,
                             EGLConfig *configs, EGLint config_size,
                             EGLint *num_config);
EGLBoolean  eglCopyBuffers (EGLDisplay dpy, EGLSurface surface,
                            EGLNativePixmapType target);
EGLContext  eglCreateContext (EGLDisplay dpy, EGLConfig config,
                              EGLContext share_context, const EGLint *attrib_list);
EGLSurface  eglCreatePbufferSurface (EGLDisplay dpy, EGLConfig config,
                                     const EGLint *attrib_list);
EGLSurface  eglCreatePixmapSurface (EGLDisplay dpy, EGLConfig config,
                                    EGLNativePixmapType pixmap,
                                    const EGLint *attrib_list);
EGLSurface  eglCreateWindowSurface (EGLDisplay dpy, EGLConfig config,
                                    EGLNativeWindowType win,
                                    const EGLint *attrib_list);
EGLBoolean  eglDestroyContext (EGLDisplay dpy, EGLContext ctx);
EGLBoolean  eglDestroySurface (EGLDisplay dpy, EGLSurface surface);
EGLBoolean  eglGetConfigAttrib (EGLDisplay dpy, EGLConfig config,
                                EGLint attribute, EGLint *value);
EGLDisplay  eglGetCurrentDisplay (void);
EGLDisplay  eglGetDisplay (EGLNativeDisplayType display_id);
EGLint      eglGetError (void);
EGLBoolean  eglInitialize (EGLDisplay dpy, EGLint *major, EGLint *minor);
EGLBoolean  eglMakeCurrent (EGLDisplay dpy, EGLSurface draw, EGLSurface read,
                            EGLContext ctx);
EGLBoolean  eglQueryContext (EGLDisplay dpy, EGLContext ctx,
                             EGLint attribute, EGLint *value);
const char *eglQueryString (EGLDisplay dpy, EGLint name);
EGLBoolean  eglQuerySurface (EGLDisplay dpy, EGLSurface surface,
                             EGLint attribute, EGLint *value);
EGLBoolean  eglSwapBuffers (EGLDisplay dpy, EGLSurface surface);
EGLBoolean  eglTerminate (EGLDisplay dpy);
EGLBoolean  eglWaitNative (EGLint engine);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* __egl_h_ */
