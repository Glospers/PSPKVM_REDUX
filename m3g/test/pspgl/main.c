/*
 * pspgl proof-of-concept: spinning lit colour cube via OpenGL ES 1.x over
 * sceGu/sceGe, using the pspdev-supplied pspgl (libGL.a + <GLES/gl.h>).
 *
 * Purpose: de-risk the M3G rendering backend decision.  If this draws, GL ES
 * 1.x is a viable substrate for m3gcore on PSP under this toolchain.
 *
 * Deliberately exercises entry points that pspgl does NOT ship and that we
 * supply in gles1x_shim.c / egl_shim.c, so a successful run proves the shim
 * approach works end to end, not just that pspgl works.
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspdebug.h>

#include <GLES/gl.h>
#include <GLES/egl.h>
#include "pspgl_shims.h"

#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("PSPGLTEST", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

#define F2X(f) ((GLfixed)((f) * 65536.0f))

static int g_running = 1;

static int exit_callback(int arg1, int arg2, void *common)
{
	(void)arg1; (void)arg2; (void)common;
	g_running = 0;
	sceKernelExitGame();
	return 0;
}

static int callback_thread(SceSize args, void *argp)
{
	(void)args; (void)argp;
	sceKernelRegisterExitCallback(
		sceKernelCreateCallback("Exit Callback", exit_callback, NULL));
	sceKernelSleepThreadCB();
	return 0;
}

static void setup_callbacks(void)
{
	int th = sceKernelCreateThread("update_thread", callback_thread,
				       0x11, 0xFA0, 0, 0);
	if (th >= 0)
		sceKernelStartThread(th, 0, 0);
}

/* ------------------------------------------------------------------ */
/* cube geometry: 6 faces x 2 tris x 3 verts, per-vertex colour        */
/* ------------------------------------------------------------------ */

#define V 0.8f
static const GLfloat cube_v[] = {
	/* +Z */ -V,-V, V,  V,-V, V,  V, V, V,   -V,-V, V,  V, V, V, -V, V, V,
	/* -Z */  V,-V,-V, -V,-V,-V, -V, V,-V,    V,-V,-V, -V, V,-V,  V, V,-V,
	/* +X */  V,-V, V,  V,-V,-V,  V, V,-V,    V,-V, V,  V, V,-V,  V, V, V,
	/* -X */ -V,-V,-V, -V,-V, V, -V, V, V,   -V,-V,-V, -V, V, V, -V, V,-V,
	/* +Y */ -V, V, V,  V, V, V,  V, V,-V,   -V, V, V,  V, V,-V, -V, V,-V,
	/* -Y */ -V,-V,-V,  V,-V,-V,  V,-V, V,   -V,-V,-V,  V,-V, V, -V,-V, V,
};
static const GLfloat cube_n[] = {
	 0, 0, 1,  0, 0, 1,  0, 0, 1,   0, 0, 1,  0, 0, 1,  0, 0, 1,
	 0, 0,-1,  0, 0,-1,  0, 0,-1,   0, 0,-1,  0, 0,-1,  0, 0,-1,
	 1, 0, 0,  1, 0, 0,  1, 0, 0,   1, 0, 0,  1, 0, 0,  1, 0, 0,
	-1, 0, 0, -1, 0, 0, -1, 0, 0,  -1, 0, 0, -1, 0, 0, -1, 0, 0,
	 0, 1, 0,  0, 1, 0,  0, 1, 0,   0, 1, 0,  0, 1, 0,  0, 1, 0,
	 0,-1, 0,  0,-1, 0,  0,-1, 0,   0,-1, 0,  0,-1, 0,  0,-1, 0,
};
static GLubyte cube_c[36 * 4];

static const GLubyte face_col[6][4] = {
	{ 255,  40,  40, 255 },	 /* +Z red    */
	{  40, 255,  40, 255 },	 /* -Z green  */
	{  60, 110, 255, 255 },	 /* +X blue   */
	{ 255, 230,  40, 255 },	 /* -X yellow */
	{ 255,  60, 235, 255 },	 /* +Y magenta*/
	{  40, 240, 240, 255 },	 /* -Y cyan   */
};

static void build_colours(void)
{
	int f, i;
	for (f = 0; f < 6; f++)
		for (i = 0; i < 6; i++)
			memcpy(&cube_c[(f * 6 + i) * 4], face_col[f], 4);
}

/* ------------------------------------------------------------------ */

static EGLDisplay dpy;
static EGLSurface surf;
static EGLContext ctx;

static const char *egl_err(void)
{
	static char buf[32];
	sprintf(buf, "0x%04X", (unsigned)eglGetError());
	return buf;
}

static void die(const char *what)
{
	pspDebugScreenInit();
	pspDebugScreenPrintf("pspgl init FAILED at %s (eglGetError=%s)\n",
			     what, egl_err());
	pspDebugScreenPrintf("press HOME to quit\n");
	while (g_running)
		sceDisplayWaitVblankStart();
	sceKernelExitGame();
}

static int gl_init(void)
{
	EGLint ncfg = 0;
	EGLConfig cfg;
	static const EGLint attribs[] = {
		EGL_RED_SIZE,   8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE,  8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 16,
		EGL_NONE
	};

	dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (dpy == EGL_NO_DISPLAY)
		die("eglGetDisplay");

	if (!eglInitialize(dpy, NULL, NULL))
		die("eglInitialize");

	/* shim entry point -- must succeed */
	if (!eglBindAPI(EGL_OPENGL_ES_API))
		die("eglBindAPI (shim)");

	if (!eglChooseConfig(dpy, attribs, &cfg, 1, &ncfg) || ncfg < 1)
		die("eglChooseConfig");

	ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, NULL);
	if (ctx == EGL_NO_CONTEXT)
		die("eglCreateContext");

	surf = eglCreateWindowSurface(dpy, cfg, 0, NULL);
	if (surf == EGL_NO_SURFACE)
		die("eglCreateWindowSurface");

	if (!eglMakeCurrent(dpy, surf, surf, ctx))
		die("eglMakeCurrent");

	return 1;
}

int main(void)
{
	float angle = 0.0f;
	EGLint w = 0, h = 0;
	GLfixed fogcol[4];

	setup_callbacks();
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	build_colours();
	gl_init();

	/* shim entry points -- prove they link and behave */
	eglQuerySurface(dpy, surf, EGL_WIDTH, &w);
	eglQuerySurface(dpy, surf, EGL_HEIGHT, &h);
	if (w <= 0) w = 480;
	if (h <= 0) h = 272;

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	/* shim */
	glActiveTexture(GL_TEXTURE0);				/* shim */
	glClientActiveTexture(GL_TEXTURE0);			/* shim */

	/* deep blue background, set through the fixed-point shim */
	glClearColorx(F2X(0.05f), F2X(0.10f), F2X(0.35f), F2X(1.0f));
	glClearDepthx(F2X(1.0f));

	fogcol[0] = F2X(0.05f);
	fogcol[1] = F2X(0.10f);
	fogcol[2] = F2X(0.35f);
	fogcol[3] = F2X(1.0f);
	glFogxv(GL_FOG_COLOR, fogcol);		/* shim */

	glViewport(0, 0, w, h);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
	glShadeModel(GL_SMOOTH);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	{
		float aspect = (float)w / (float)h;
		glFrustumf(-0.5f * aspect, 0.5f * aspect, -0.5f, 0.5f, 1.0f, 100.0f);
	}
	glMatrixMode(GL_MODELVIEW);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, cube_v);
	glNormalPointer(GL_FLOAT, 0, cube_n);
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, cube_c);

	while (g_running) {
		SceCtrlData pad;

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glLoadIdentity();
		glTranslatef(0.0f, 0.0f, -4.0f);
		glRotatef(angle,        1.0f, 0.0f, 0.0f);
		glRotatef(angle * 1.7f, 0.0f, 1.0f, 0.0f);
		glRotatef(angle * 0.3f, 0.0f, 0.0f, 1.0f);

		glDrawArrays(GL_TRIANGLES, 0, 36);

		/* a flat 2D bar along the bottom, positioned with the
		 * fixed-point ortho shim -- also proves glColor4x. */
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrthox(0, F2X(480.0f), 0, F2X(272.0f), F2X(-1.0f), F2X(1.0f));
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glDisable(GL_DEPTH_TEST);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
		{
			static const GLfloat bar[] = {
				 10.0f, 10.0f, 0.0f,  470.0f, 10.0f, 0.0f,
				470.0f, 30.0f, 0.0f,   10.0f, 10.0f, 0.0f,
				470.0f, 30.0f, 0.0f,   10.0f, 30.0f, 0.0f,
			};
			glColor4x(F2X(1.0f), F2X(0.55f), F2X(0.0f), F2X(1.0f));
			glVertexPointer(3, GL_FLOAT, 0, bar);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glVertexPointer(3, GL_FLOAT, 0, cube_v);
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		}
		glEnableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);
		glEnable(GL_DEPTH_TEST);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);

		eglSwapBuffers(dpy, surf);

		angle += 1.2f;
		if (angle >= 360.0f)
			angle -= 360.0f;

		sceCtrlPeekBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_START)
			g_running = 0;
	}

	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(dpy, surf);
	eglDestroyContext(dpy, ctx);
	eglTerminate(dpy);

	sceKernelExitGame();
	return 0;
}
