/*
 * egl_shim.c -- EGL entry points declared by pspgl's <GLES/egl.h> but not
 * present in libGL.a.  Verified missing with psp-nm:
 *   eglBindAPI eglQuerySurface eglQueryContext eglGetCurrentDisplay
 *   eglCopyBuffers eglCreatePixmapSurface
 *
 * pspgl keeps its EGL state in file-static structs we cannot reach from
 * outside, so these are best-effort: enough to link and to satisfy the
 * bookkeeping calls m3gcore makes, not a real EGL 1.1 implementation.
 */

#include <GLES/egl.h>

#include "pspgl_shims.h"

/* pspgl's egltypes.h predates EGL 1.2 and has no EGLenum. */
typedef unsigned int EGLenum;

/* PSP display is fixed at 480x272. */
#define PSP_SCREEN_W 480
#define PSP_SCREEN_H 272

EGLBoolean eglBindAPI(EGLenum api)
{
	return (api == EGL_OPENGL_ES_API) ? EGL_TRUE : EGL_FALSE;
}

EGLBoolean eglQuerySurface(EGLDisplay dpy, EGLSurface surface,
			   EGLint attribute, EGLint *value)
{
	(void)dpy;
	if (!surface || !value)
		return EGL_FALSE;

	switch (attribute) {
	case EGL_WIDTH:  *value = PSP_SCREEN_W; return EGL_TRUE;
	case EGL_HEIGHT: *value = PSP_SCREEN_H; return EGL_TRUE;
	default:         return EGL_FALSE;
	}
}

EGLBoolean eglQueryContext(EGLDisplay dpy, EGLContext ctx,
			   EGLint attribute, EGLint *value)
{
	(void)dpy;
	if (!ctx || !value)
		return EGL_FALSE;

	switch (attribute) {
	case EGL_CONTEXT_CLIENT_VERSION: *value = 1; return EGL_TRUE;
	default:                         return EGL_FALSE;
	}
}

EGLDisplay eglGetCurrentDisplay(void)
{
	/* pspgl has exactly one display and eglGetDisplay ignores its argument. */
	return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

EGLBoolean eglCopyBuffers(EGLDisplay dpy, EGLSurface surface,
			  NativePixmapType target)
{
	(void)dpy; (void)surface; (void)target;
	return EGL_FALSE;	/* no native pixmaps on PSP */
}

EGLSurface eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config,
				  NativePixmapType pixmap,
				  const EGLint *attrib_list)
{
	(void)dpy; (void)config; (void)pixmap; (void)attrib_list;
	return EGL_NO_SURFACE;	/* no native pixmaps on PSP */
}
