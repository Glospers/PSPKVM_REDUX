/*
 * m3g_psp_gl.c -- the GL ES 1.x entry points pspgl does not implement.
 *
 * The PSP backend is pspgl (libGL.a, BSD-3-Clause, shipped with the pspdev
 * toolchain at /usr/local/pspdev/psp/lib).  It covers 53 of the 63 GL ES 1.x
 * functions m3gcore calls; the ten below are absent from the archive and show
 * up only as undefined references at link time, never as compile errors,
 * because the headers declare the full API either way.
 *
 * Verified missing with psp-nm on libGL.a:
 *   glColor4x glClearColorx glClearDepthx glOrthox glTexEnvx glTexParameterx
 *   glFogxv glActiveTexture glClientActiveTexture glHint
 *
 * Each is a thin forward to the float entry point pspgl does implement.  The
 * fixed-point conversion is the only place there is anything to get wrong, and
 * it is wrong in two directions:
 *
 *   - GL ES fixed point is 16.16, so a genuine scalar divides by 65536; but
 *   - a pname whose value is an *enum* or a *boolean* is not fixed point at
 *     all and must pass through as an integer.  glTexParameterx has no
 *     fixed-point pname in ES 1.x at all, and glTexEnvx/glFogxv have a mix.
 *
 * These were proven end to end before being brought into the build: the
 * standalone cube in test/pspgl/ deliberately routes its clear colour, its 2D
 * projection and its flat colour through the shimmed entry points, and renders
 * correctly under PPSSPP.  See test/pspgl/ASSESSMENT.md.
 *
 * This translation unit compiles against m3g/inc/GLES/gl.h, the same header
 * m3gcore sees.  That header declares only the subset m3gcore uses, so the
 * float entry points forwarded to below are declared here instead; the
 * prototypes are the standard Khronos ones and match pspgl's exactly.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include <GLES/gl.h>

/*----------------------------------------------------------------------
 * pspgl entry points forwarded to
 *--------------------------------------------------------------------*/

extern void glColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
extern void glClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
extern void glClearDepthf (GLclampf depth);
extern void glOrthof (GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
                      GLfloat zNear, GLfloat zFar);
extern void glTexEnvf (GLenum target, GLenum pname, GLfloat param);
extern void glFogfv (GLenum pname, const GLfloat *params);

/* OpenGL ES fixed point is 16.16 */
#define X2F(x) ((GLfloat) (x) * (1.0f / 65536.0f))

/* m3g/inc/GLES/gl.h carries only the tokens m3gcore names, and it never names
 * these two -- they are needed here to tell the one fixed-point glTexEnv pname
 * pair apart from the enum-valued ones.  Standard Khronos values. */
#ifndef GL_RGB_SCALE
#define GL_RGB_SCALE                      0x8573
#endif
#ifndef GL_ALPHA_SCALE
#define GL_ALPHA_SCALE                    0x0D1C
#endif

/*----------------------------------------------------------------------
 * Fixed-point forwards
 *--------------------------------------------------------------------*/

void glColor4x (GLfixed r, GLfixed g, GLfixed b, GLfixed a)
{
    glColor4f(X2F(r), X2F(g), X2F(b), X2F(a));
}

void glClearColorx (GLclampx r, GLclampx g, GLclampx b, GLclampx a)
{
    glClearColor(X2F(r), X2F(g), X2F(b), X2F(a));
}

void glClearDepthx (GLclampx depth)
{
    glClearDepthf(X2F(depth));
}

void glOrthox (GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)
{
    glOrthof(X2F(l), X2F(r), X2F(b), X2F(t), X2F(n), X2F(f));
}

void glTexEnvx (GLenum target, GLenum pname, GLfixed param)
{
    /* GL_TEXTURE_ENV_MODE takes an enum, not a fixed-point scalar; only
     * GL_RGB_SCALE / GL_ALPHA_SCALE are genuinely fixed point. */
    switch (pname) {
    case GL_RGB_SCALE:
    case GL_ALPHA_SCALE:
        glTexEnvf(target, pname, X2F(param));
        break;
    default:
        glTexEnvf(target, pname, (GLfloat) param);
        break;
    }
}

void glTexParameterx (GLenum target, GLenum pname, GLfixed param)
{
    /* Every ES 1.x glTexParameter pname is an enum or a boolean, so the value
     * passes through as an integer -- scaling it here would turn GL_LINEAR
     * into zero. */
    glTexParameteri(target, pname, (GLint) param);
}

void glFogxv (GLenum pname, const GLfixed *params)
{
    GLfloat f[4];
    int n = (pname == GL_FOG_COLOR) ? 4 : 1;
    int i;

    for (i = 0; i < n; ++i) {
        /* GL_FOG_MODE is an enum; density, start and end are real scalars. */
        f[i] = (pname == GL_FOG_MODE) ? (GLfloat) params[i] : X2F(params[i]);
    }
    glFogfv(pname, f);
}

/*----------------------------------------------------------------------
 * No-ops
 *--------------------------------------------------------------------*/

void glActiveTexture (GLenum texture)
{
    /* The PSP GE has exactly one texture unit and pspgl exposes exactly one,
     * so selecting unit 0 is a no-op and anything else is an error the engine
     * never commits: m3gcore is built with M3G_NUM_TEXTURE_UNITS == 1. */
    (void) texture;
}

void glClientActiveTexture (GLenum texture)
{
    (void) texture;
}

void glHint (GLenum target, GLenum mode)
{
    /* Hints are advisory by definition and the GE has no equivalent knob. */
    (void) target;
    (void) mode;
}
