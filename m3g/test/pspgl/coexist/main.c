/*
 * coexist -- does pspgl survive alongside an application that already drives
 * the GE through libpspgu (sceGu*), the way PSPKVM does?
 *
 * PSPKVM sets the GE up once at startup in psp/pspkvm.c:364-392
 *   fbp0 = getStaticVramBuffer(512, 272, GU_PSM_8888);   -- edram +0x000000
 *   fbp1 = getStaticVramBuffer(512, 272, GU_PSM_8888);   -- edram +0x088000
 *   zbp  = getStaticVramBuffer(512, 272, GU_PSM_4444);   -- edram +0x110000
 *   sceGuInit(); sceGuDrawBuffer/DispBuffer/DepthBuffer; ...
 * and then blits the MIDP 16-bit screen buffer as a texture every flush
 * (javacall/implementation/psp_mips/midp/lcd.c:121-138), ending with
 * sceGuSwapBuffers().
 *
 * This reproduces that exact setup, then brings pspgl up on top of it and
 * interleaves the two: a sceGu 2D pass and a pspgl 3D pass, alternating.
 * Whatever happens on screen is the answer.
 *
 * Progress is reported with printf so it lands in PPSSPP's log rather than
 * on the framebuffer we are trying to observe.
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspge.h>
#include <pspctrl.h>

#include <GLES/gl.h>
#include <GLES/egl.h>

#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("PSPGLCOEX", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

#define BUF_WIDTH  512
#define SCR_WIDTH  480
#define SCR_HEIGHT 272

static unsigned int __attribute__((aligned(16))) gu_list[262144];
static int g_running = 1;

static int exit_callback(int a, int b, void *c)
{
	(void)a; (void)b; (void)c;
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

/* PSPKVM's psp/vram.c allocator: hands out sequential offsets from edram 0. */
static unsigned int vram_off = 0;
static void *static_vram(unsigned int w, unsigned int h, unsigned int psm)
{
	unsigned int bpp = (psm == GU_PSM_T8) ? 1 : (psm == GU_PSM_8888) ? 4 : 2;
	void *p = (void *)vram_off;
	vram_off += w * h * bpp;
	return p;
}

struct Vertex { float u, v; unsigned short color; float x, y, z; };

int main(void)
{
	void *fbp0, *fbp1, *zbp;
	EGLDisplay dpy;
	EGLSurface surf;
	EGLContext ctx;
	EGLConfig cfg;
	EGLint ncfg = 0;
	int frame = 0;
	float angle = 0.0f;
	static const EGLint attribs[] = {
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE
	};
	static const float tri[] = {
		 0.0f,  0.9f, 0.0f,
		-0.9f, -0.7f, 0.0f,
		 0.9f, -0.7f, 0.0f,
	};
	static const unsigned char tri_c[] = {
		255,  60,  60, 255,
		 60, 255,  60, 255,
		 80, 130, 255, 255,
	};

	{
		int th = sceKernelCreateThread("cb", callback_thread, 0x11, 0xFA0, 0, 0);
		if (th >= 0) sceKernelStartThread(th, 0, 0);
	}
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	printf("COEX: edram base=%p size=%u\n",
	       sceGeEdramGetAddr(), (unsigned)sceGeEdramGetSize());

	/* ---- 1. exactly what PSPKVM does at startup ---- */
	fbp0 = static_vram(BUF_WIDTH, SCR_HEIGHT, GU_PSM_8888);
	fbp1 = static_vram(BUF_WIDTH, SCR_HEIGHT, GU_PSM_8888);
	zbp  = static_vram(BUF_WIDTH, SCR_HEIGHT, GU_PSM_4444);
	printf("COEX: sceGu buffers fbp0=+0x%06X fbp1=+0x%06X zbp=+0x%06X used=0x%06X\n",
	       (unsigned)fbp0, (unsigned)fbp1, (unsigned)zbp, vram_off);

	sceGuInit();
	sceGuStart(GU_DIRECT, gu_list);
	sceGuDrawBuffer(GU_PSM_8888, fbp0, BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, fbp1, BUF_WIDTH);
	sceGuDepthBuffer(zbp, BUF_WIDTH);
	sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
	sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
	sceGuDepthRange(0xc350, 0x2710);
	sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFrontFace(GU_CW);
	sceGuDepthFunc(GU_GEQUAL);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuFinish();
	sceGuSync(0, 0);
	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);
	printf("COEX: sceGuInit done\n");

	/* draw a few pure-sceGu frames first, so we know that path works */
	for (frame = 0; frame < 30 && g_running; frame++) {
		sceGuStart(GU_DIRECT, gu_list);
		sceGuClearColor(0xFF206020);	/* ABGR: dark green */
		sceGuClearDepth(0);
		sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
		sceGuFinish();
		sceGuSync(0, 0);
		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}
	printf("COEX: 30 pure sceGu frames OK\n");

	/* ---- 2. bring pspgl up on top of the live sceGu context ---- */
	dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	printf("COEX: eglGetDisplay -> %p\n", dpy);
	if (!eglInitialize(dpy, NULL, NULL)) {
		printf("COEX: eglInitialize FAILED err=0x%04X\n", (unsigned)eglGetError());
		goto done;
	}
	printf("COEX: eglInitialize OK\n");
	if (!eglChooseConfig(dpy, attribs, &cfg, 1, &ncfg) || ncfg < 1) {
		printf("COEX: eglChooseConfig FAILED err=0x%04X\n", (unsigned)eglGetError());
		goto done;
	}
	ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, NULL);
	printf("COEX: eglCreateContext -> %p err=0x%04X\n", ctx, (unsigned)eglGetError());
	surf = eglCreateWindowSurface(dpy, cfg, 0, NULL);
	printf("COEX: eglCreateWindowSurface -> %p err=0x%04X\n", surf, (unsigned)eglGetError());
	if (surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT)
		goto done;
	if (!eglMakeCurrent(dpy, surf, surf, ctx)) {
		printf("COEX: eglMakeCurrent FAILED err=0x%04X\n", (unsigned)eglGetError());
		goto done;
	}
	printf("COEX: eglMakeCurrent OK -- GL_RENDERER=%s\n",
	       (const char *)glGetString(GL_RENDERER));

	/* Set smooth shading ONCE, at init, exactly as a well-behaved GL client
	 * would.  If the triangle still comes out flat-shaded, pspgl's dirty-
	 * register cache was invalidated behind its back by the sceGu state
	 * block and it never re-emitted the value. */
	glShadeModel(GL_SMOOTH);
	glClearColor(0.05f, 0.10f, 0.35f, 1.0f);
	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
	glDisable(GL_DEPTH_TEST);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, tri);
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, tri_c);

	/* ---- 3. interleave: GL frame, then a sceGu frame, forever ---- */
	printf("COEX: entering interleaved loop\n");
	for (frame = 0; g_running; frame++) {
		SceCtrlData pad;

		if ((frame & 1) == 0) {
			glClear(GL_COLOR_BUFFER_BIT);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glRotatef(angle, 0.0f, 0.0f, 1.0f);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			eglSwapBuffers(dpy, surf);
		} else {
			sceGuStart(GU_DIRECT, gu_list);
			sceGuClearColor(0xFF206020);
			sceGuClear(GU_COLOR_BUFFER_BIT);
			sceGuFinish();
			sceGuSync(0, 0);
			sceDisplayWaitVblankStart();
			sceGuSwapBuffers();
		}

		angle += 1.5f;
		if (frame == 60)
			printf("COEX: survived 60 interleaved frames\n");
		if (frame == 600)
			printf("COEX: survived 600 interleaved frames\n");

		sceCtrlPeekBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_START)
			g_running = 0;
	}

done:
	printf("COEX: exiting\n");
	sceKernelExitGame();
	return 0;
}
