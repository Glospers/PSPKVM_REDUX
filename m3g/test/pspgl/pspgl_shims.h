/*
 * Declarations for the EGL entry points pspgl's <GLES/egl.h> omits.
 * (Its <GLES/gl.h> is the full Khronos header, so the GL-side shims in
 * gles1x_shim.c need no extra declarations -- only implementations.)
 */
#ifndef PSPGL_SHIMS_H
#define PSPGL_SHIMS_H

#include <GLES/egl.h>

#define EGL_OPENGL_ES_API          0x30A0
#define EGL_HEIGHT                 0x3056
#define EGL_WIDTH                  0x3057
#define EGL_CONTEXT_CLIENT_VERSION 0x3098

EGLBoolean eglBindAPI(unsigned int api);
EGLBoolean eglQuerySurface(EGLDisplay dpy, EGLSurface surface,
			   EGLint attribute, EGLint *value);
EGLBoolean eglQueryContext(EGLDisplay dpy, EGLContext ctx,
			   EGLint attribute, EGLint *value);
EGLDisplay eglGetCurrentDisplay(void);
EGLBoolean eglCopyBuffers(EGLDisplay dpy, EGLSurface surface,
			  NativePixmapType target);
EGLSurface eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config,
				  NativePixmapType pixmap,
				  const EGLint *attrib_list);

#endif /* PSPGL_SHIMS_H */
