/*
 * gles1x_shim.c -- the GL ES 1.x entry points that m3gcore calls but pspgl's
 * libGL.a does not define.  pspgl ships the full Khronos <GLES/gl.h>, so every
 * one of these is already *declared*; only the implementation is absent, which
 * shows up as an undefined reference at link time.  Each is a thin forward to
 * the float entry point pspgl does implement.
 *
 * Verified missing from /usr/local/pspdev/psp/lib/libGL.a (psp-nm):
 *   glColor4x glClearColorx glClearDepthx glOrthox glTexEnvx glTexParameterx
 *   glFogxv glActiveTexture glClientActiveTexture glHint
 */

#include <GLES/gl.h>

/* OpenGL ES fixed point is 16.16 */
#define X2F(x) ((GLfloat)(x) * (1.0f / 65536.0f))

void glColor4x(GLfixed r, GLfixed g, GLfixed b, GLfixed a)
{
	glColor4f(X2F(r), X2F(g), X2F(b), X2F(a));
}

void glClearColorx(GLclampx r, GLclampx g, GLclampx b, GLclampx a)
{
	glClearColor(X2F(r), X2F(g), X2F(b), X2F(a));
}

void glClearDepthx(GLclampx depth)
{
	glClearDepthf(X2F(depth));
}

void glOrthox(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)
{
	glOrthof(X2F(l), X2F(r), X2F(b), X2F(t), X2F(n), X2F(f));
}

void glTexEnvx(GLenum target, GLenum pname, GLfixed param)
{
	/* GL_TEXTURE_ENV_MODE takes an enum, not a fixed-point scalar; only
	 * GL_RGB_SCALE / GL_ALPHA_SCALE are genuinely fixed-point. */
	switch (pname) {
	case GL_RGB_SCALE:
	case GL_ALPHA_SCALE:
		glTexEnvf(target, pname, X2F(param));
		break;
	default:
		glTexEnvf(target, pname, (GLfloat)param);
		break;
	}
}

void glTexParameterx(GLenum target, GLenum pname, GLfixed param)
{
	/* Every ES 1.x glTexParameter pname is an enum or a boolean, so the
	 * value passes through as an integer, not as 16.16. */
	glTexParameteri(target, pname, (GLint)param);
}

void glFogxv(GLenum pname, const GLfixed *params)
{
	GLfloat f[4];
	int n = (pname == GL_FOG_COLOR) ? 4 : 1;
	int i;

	for (i = 0; i < n; i++) {
		/* GL_FOG_MODE is an enum; everything else is a real scalar. */
		f[i] = (pname == GL_FOG_MODE) ? (GLfloat)params[i] : X2F(params[i]);
	}
	glFogfv(pname, f);
}

void glActiveTexture(GLenum texture)
{
	/* PSP GE has exactly one texture unit, and pspgl exposes exactly one.
	 * Selecting unit 0 is a no-op; anything else is an error. */
	(void)texture;
}

void glClientActiveTexture(GLenum texture)
{
	(void)texture;
}

void glHint(GLenum target, GLenum mode)
{
	/* Hints are advisory by definition; the GE has no equivalent knob. */
	(void)target;
	(void)mode;
}
