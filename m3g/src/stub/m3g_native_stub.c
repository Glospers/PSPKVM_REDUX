/*
 * m3g_native_stub.c -- native render-surface hooks.
 *
 * inc/m3g_gl.h:82-99 declares four functions that every platform port of
 * m3gcore has to supply; they map the opaque M3GNativeBitmap / M3GNativeWindow
 * handles that come in through m3gBindBitmapTarget() and m3gBindWindowTarget()
 * onto whatever the host windowing system actually uses.  The Symbian port
 * implements them over CFbsBitmap (src/m3g_symbian_gl.cpp:62-81).
 *
 * The PSP has no such handle space, and the PSPKVM path to the screen will go
 * through m3gBindMemoryTarget() (M3G/m3g_core.h:1145) -- a plain pixel buffer,
 * which needs none of these hooks.  They are therefore refused rather than
 * faked:
 *
 *   src/m3g_rendercontext.inl:1450 and :1573 turn a false return into
 *   m3gRaiseError(M3G_INVALID_OBJECT), which surfaces in Java as a failed
 *   Graphics3D.bindTarget().  That is the correct answer for "this platform
 *   has no native bitmap/window targets".
 *
 * m3gglLockNativeBitmap is only ever reached after
 * m3gglGetNativeBitmapParams has succeeded (the target has to be bound first),
 * so it cannot be called while the two above return false.  It refuses too.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include "M3G/m3g_core.h"

M3Gbool m3gglLockNativeBitmap (M3GNativeBitmap bitmap,
                               M3Gubyte **ptr,
                               M3Gsizei *stride)
{
    (void) bitmap;
    if (ptr != 0)    { *ptr = 0; }
    if (stride != 0) { *stride = 0; }
    return M3G_FALSE;
}

void m3gglReleaseNativeBitmap (M3GNativeBitmap bitmap)
{
    (void) bitmap;
}

M3Gbool m3gglGetNativeBitmapParams (M3GNativeBitmap bitmap,
                                    M3GPixelFormat *format,
                                    M3Gint *width, M3Gint *height, M3Gint *pixels)
{
    (void) bitmap;
    if (format != 0) { *format = M3G_NO_FORMAT; }
    if (width  != 0) { *width  = 0; }
    if (height != 0) { *height = 0; }
    if (pixels != 0) { *pixels = 0; }
    return M3G_FALSE;
}

M3Gbool m3gglGetNativeWindowParams (M3GNativeWindow wnd,
                                    M3GPixelFormat *format,
                                    M3Gint *width, M3Gint *height)
{
    (void) wnd;
    if (format != 0) { *format = M3G_NO_FORMAT; }
    if (width  != 0) { *width  = 0; }
    if (height != 0) { *height = 0; }
    return M3G_FALSE;
}
