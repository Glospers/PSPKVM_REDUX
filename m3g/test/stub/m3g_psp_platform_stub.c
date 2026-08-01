/*
 * m3g_psp_platform_stub.c -- the PSP-only half of the port, for the host.
 *
 * m3gPspGetInterface reserves video memory and brings EGL up before it creates
 * the engine interface, because on the PSP both have to happen before pspgl
 * touches the GE (see ../../src/m3g_psp_loader.c).  Neither exists on the build
 * machine, and neither has anything to do with parsing a file, so the harness
 * links these instead of src/m3g_psp_vidmem.c and src/m3g_psp_egl.c.
 *
 * Reporting success is the right answer here: the callers treat a refusal as a
 * reason to stop, and there is nothing to refuse when the GL below is itself a
 * stub (stub/m3g_gl_stub.c).
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include "M3G/m3g_psp.h"

M3Gint m3gPspReserveVram(void)
{
    return 1;
}

M3Gint m3gPspGetReservedVram(void)
{
    return 0;
}

int m3gPspHoldGLContext(void)
{
    return 1;
}

void m3gPspReadbackHint(M3Gint x, M3Gint y, M3Gint width, M3Gint height)
{
    /* The read-back corrector lives in src/m3g_psp_gl.c, which the harness
     * replaces with the stub GL; there is nothing to hint. */
    (void) x; (void) y; (void) width; (void) height;
}
