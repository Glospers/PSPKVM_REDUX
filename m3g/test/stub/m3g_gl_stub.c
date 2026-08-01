/*
 * m3g_gl_stub.c -- no-op OpenGL ES 1.1 backend.
 *
 * Phase 2 step 1 only gets the M3G engine compiling and linking on the PSP; it
 * does not draw anything.  m3gcore has no software rasteriser to fall back on
 * (src/m3g_rendercontext.inl:25 #error's the NGL software path out), so every
 * GL entry point it references has to exist at link time or the ELF will not
 * link.  This file supplies them as no-ops.
 *
 * The bodies are deliberately not empty everywhere: the few functions whose
 * return value the engine actually inspects return values that keep it on its
 * normal code path, so that when the engine is first exercised at runtime it
 * fails to *draw* rather than failing to *run*:
 *
 *   glGetError()    GL_NO_ERROR    -- m3gAssertGL (inc/m3g_gl.h:55) and the
 *                                     out-of-memory checks around texture
 *                                     upload treat anything else as fatal.
 *   glGetString()   a real string  -- src/m3g_interface.c:1290 immediately
 *                                     calls strstr() on the result; NULL would
 *                                     dereference.  "SW" (not "HW") is
 *                                     deliberate: it is how m3gcore is told the
 *                                     renderer has no antialiasing, and it
 *                                     avoids the MBX-specific workarounds.
 *   glGetIntegerv() sane limits    -- queried for GL_MAX_TEXTURE_SIZE and
 *                                     GL_MAX_VIEWPORT_DIMS during interface
 *                                     creation; zero would clamp every
 *                                     viewport and texture to nothing.
 *   glGenTextures() rising ids     -- 0 is "no texture" in GL, and the engine
 *                                     stores the name it gets back.
 *
 * The PSP screen is 480x272 and the toolchain's own GU limit for textures is
 * 512, so those are the numbers reported here.  m3g_config.h claims 1024 for
 * both (M3G_MAX_VIEWPORT_DIMENSION / M3G_MAX_TEXTURE_DIMENSION); the engine
 * takes the minimum of its own constant and what GL reports.
 *
 * When the sceGu backend arrives it replaces this file wholesale; nothing else
 * in the build has to change.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include <GLES/gl.h>

/* Reported hardware limits. 512 is the PSP's maximum texture dimension;
 * 480x272 is the framebuffer. */
#define STUB_MAX_TEXTURE_SIZE   512
#define STUB_MAX_VIEWPORT_W     480
#define STUB_MAX_VIEWPORT_H     272

static GLuint stub_nextTexture = 1;

/*----------------------------------------------------------------------
 * Queries -- the only entry points whose results the engine acts on
 *--------------------------------------------------------------------*/

GLenum glGetError (void)
{
    return GL_NO_ERROR;
}

const GLubyte *glGetString (GLenum name)
{
    switch (name) {
    case GL_VENDOR:     return (const GLubyte *) "PSPKVM";
    /* Must not contain "HW" or "MBX": src/m3g_interface.c:1290-1301 keys
     * antialiasing support and two MBX errata workarounds off this string. */
    case GL_RENDERER:   return (const GLubyte *) "PSPKVM M3G null backend (SW)";
    case GL_VERSION:    return (const GLubyte *) "OpenGL ES-CM 1.1";
    case GL_EXTENSIONS: return (const GLubyte *) "";
    default:            return (const GLubyte *) "";
    }
}

void glGetIntegerv (GLenum pname, GLint *params)
{
    if (params == 0) {
        return;
    }
    switch (pname) {
    case GL_MAX_TEXTURE_SIZE:
        params[0] = STUB_MAX_TEXTURE_SIZE;
        break;
    case GL_MAX_VIEWPORT_DIMS:
        params[0] = STUB_MAX_VIEWPORT_W;
        params[1] = STUB_MAX_VIEWPORT_H;
        break;
    default:
        params[0] = 0;
        break;
    }
}

void glGenTextures (GLsizei n, GLuint *textures)
{
    GLsizei i;
    if (textures == 0) {
        return;
    }
    for (i = 0; i < n; ++i) {
        textures[i] = stub_nextTexture++;
    }
}

void glDeleteTextures (GLsizei n, const GLuint *textures)
{
    (void) n; (void) textures;
}

void glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height,
                   GLenum format, GLenum type, GLvoid *pixels)
{
    (void) x; (void) y; (void) width; (void) height;
    (void) format; (void) type; (void) pixels;
}

/*----------------------------------------------------------------------
 * Everything else -- accepted and discarded
 *--------------------------------------------------------------------*/

void glActiveTexture (GLenum texture) { (void) texture; }
void glAlphaFunc (GLenum func, GLclampf ref) { (void) func; (void) ref; }
void glBindTexture (GLenum target, GLuint texture) { (void) target; printf("STUB bindTex %u", texture); putchar(10); }
void glBlendFunc (GLenum sfactor, GLenum dfactor) { (void) sfactor; (void) dfactor; }
void glClear (GLbitfield mask) { (void) mask; }

void glClearColorx (GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha)
{ (void) red; (void) green; (void) blue; (void) alpha; }

void glClearDepthx (GLclampx depth) { (void) depth; }
void glClientActiveTexture (GLenum texture) { (void) texture; }

void glColor4x (GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
{ (void) red; (void) green; (void) blue; (void) alpha; }

void glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{ (void) red; (void) green; (void) blue; (void) alpha; }

void glColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{ (void) stride; (void) pointer; printf("STUB colorPtr n=%d type=%04x", size, type); putchar(10); }

void glCompressedTexImage2D (GLenum target, GLint level, GLenum internalformat,
                             GLsizei width, GLsizei height, GLint border,
                             GLsizei imageSize, const GLvoid *data)
{ (void) target; (void) level; (void) internalformat; (void) width;
  (void) height; (void) border; (void) imageSize; (void) data; }

void glCopyTexImage2D (GLenum target, GLint level, GLenum internalformat,
                       GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{ (void) target; (void) level; (void) internalformat; (void) x; (void) y;
  (void) width; (void) height; (void) border; }

void glCullFace (GLenum mode) { (void) mode; }
void glDepthFunc (GLenum func) { (void) func; }
void glDepthMask (GLboolean flag) { (void) flag; }
void glDepthRangef (GLclampf zNear, GLclampf zFar) { (void) zNear; (void) zFar; }
void glDisable (GLenum cap) { printf("STUB disable %04x", cap); putchar(10); }
void glDisableClientState (GLenum array) { printf("STUB clientOff %04x", array); putchar(10); }

/*
 * Draw-call counters.
 *
 * The engine culls scene nodes on the CPU before anything reaches GL, so
 * whether a draw call arrives here at all separates "the scene was culled or
 * transformed away inside the engine" from "the triangles reached the
 * backend".  m3g_render_test.c reads these.
 */
int m3gStubDrawCalls;
int m3gStubDrawVertices;

/*
 * Matrix and vertex-array tracing (M3G_STUB_TRACE_MATRICES).
 *
 * The engine can submit draws whose composite matrices place everything
 * outside the view volume, and a draw-count harness cannot tell.  Printing
 * what reaches the backend makes the arithmetic checkable offline.
 */
#include <stdio.h>

static unsigned s_matrixMode = 0x1700;

void glMatrixMode (GLenum mode)
{
    s_matrixMode = mode;
}

static void stubPrintMatrix(const char *tag, const GLfloat *m)
{
    int i;
    printf("STUB %s mode=%04x", tag, s_matrixMode);
    for (i = 0; i < 16; ++i) {
        printf(" %g", (double) m[i]);
    }
    putchar('\n');
}

void glLoadMatrixf (const GLfloat *m) { stubPrintMatrix("load", m); }
void glMultMatrixf (const GLfloat *m) { stubPrintMatrix("mult", m); }
void glLoadIdentity (void) { printf("STUB idty mode=%04x\n", s_matrixMode); }

static const void *s_vp; static int s_vpType, s_vpStride, s_vpSize;

void glVertexPointer (GLint size, GLenum type, GLsizei stride,
                      const GLvoid *pointer)
{
    s_vp = pointer; s_vpType = type; s_vpStride = stride; s_vpSize = size;
    printf("STUB varr size=%d type=%04x stride=%d\n", size, type, stride);
}

static void stubPrintVerts(int count)
{
    int i, n = (count < 3) ? count : 3;

    if (s_vp == 0) {
        return;
    }
    for (i = 0; i < n; ++i) {
        if (s_vpType == 0x1402 /* GL_SHORT */) {
            int stride = s_vpStride ? s_vpStride : s_vpSize * 2;
            const short *v = (const short *) ((const char *) s_vp + i * stride);
            printf("STUB vtx%d %d %d %d\n", i, v[0], v[1], v[2]);
        }
        else if (s_vpType == 0x1400 /* GL_BYTE */) {
            int stride = s_vpStride ? s_vpStride : s_vpSize;
            const signed char *v =
                (const signed char *) ((const char *) s_vp + i * stride);
            printf("STUB vtx%d %d %d %d\n", i, v[0], v[1], v[2]);
        }
        else if (s_vpType == 0x1406 /* GL_FLOAT */) {
            int stride = s_vpStride ? s_vpStride : s_vpSize * 4;
            const float *v = (const float *) ((const char *) s_vp + i * stride);
            printf("STUB vtx%d %g %g %g\n", (int) i,
                   (double) v[0], (double) v[1], (double) v[2]);
        }
    }
}

void glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
    (void) mode; (void) first;
    m3gStubDrawCalls++;
    m3gStubDrawVertices += (int) count;
    printf("STUB drawArrays count=%d\n", (int) count);
    stubPrintVerts(count);
}

void glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    (void) mode; (void) type; (void) indices;
    m3gStubDrawCalls++;
    m3gStubDrawVertices += (int) count;
    printf("STUB drawElements count=%d\n", (int) count);
    stubPrintVerts(count);
}

void glEnable (GLenum cap) { printf("STUB enable %04x", cap); putchar(10); }
void glEnableClientState (GLenum array) { printf("STUB clientOn %04x", array); putchar(10); }
void glFinish (void) { }
void glFogf (GLenum pname, GLfloat param) { (void) pname; (void) param; }
void glFogxv (GLenum pname, const GLfixed *params) { (void) pname; (void) params; }
void glFrontFace (GLenum mode) { (void) mode; }
void glHint (GLenum target, GLenum mode) { (void) target; (void) mode; }
void glLightModelf (GLenum pname, GLfloat param) { (void) pname; (void) param; }
void glLightModelfv (GLenum pname, const GLfloat *params) { (void) pname; (void) params; }

void glLightf (GLenum light, GLenum pname, GLfloat param)
{ (void) light; (void) pname; (void) param; }

void glLightfv (GLenum light, GLenum pname, const GLfloat *params)
{ (void) light; (void) pname; (void) params; }


void glMaterialf (GLenum face, GLenum pname, GLfloat param)
{ (void) face; (void) pname; (void) param; }

void glMaterialfv (GLenum face, GLenum pname, const GLfloat *params)
{ (void) face; (void) pname; (void) params; }


void glNormalPointer (GLenum type, GLsizei stride, const GLvoid *pointer)
{ (void) type; (void) stride; (void) pointer; }

void glOrthox (GLfixed left, GLfixed right, GLfixed bottom, GLfixed top,
               GLfixed zNear, GLfixed zFar)
{ (void) left; (void) right; (void) bottom; (void) top; (void) zNear; (void) zFar; }

void glPixelStorei (GLenum pname, GLint param) { (void) pname; (void) param; }
void glPolygonOffset (GLfloat factor, GLfloat units) { (void) factor; (void) units; }
void glPopMatrix (void) { }
void glPushMatrix (void) { }
void glScalef (GLfloat x, GLfloat y, GLfloat z) { (void) x; (void) y; (void) z; }

void glScissor (GLint x, GLint y, GLsizei width, GLsizei height)
{ (void) x; (void) y; (void) width; (void) height; }

void glShadeModel (GLenum mode) { (void) mode; }

void glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{ (void) stride; (void) pointer; printf("STUB texcPtr n=%d type=%04x", size, type); putchar(10); }

void glTexEnvfv (GLenum target, GLenum pname, const GLfloat *params)
{ (void) target; (void) pname; (void) params; }

void glTexEnvx (GLenum target, GLenum pname, GLfixed param)
{ (void) target; (void) pname; (void) param; }

void glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{ (void) target; (void) level; (void) border; (void) pixels; printf("STUB texImage %dx%d ifmt=%04x fmt=%04x type=%04x", (int) width, (int) height, (unsigned) internalformat, format, type); putchar(10); }

void glTexParameteri (GLenum target, GLenum pname, GLint param)
{ (void) target; (void) pname; (void) param; }

void glTexParameterx (GLenum target, GLenum pname, GLfixed param)
{ (void) target; (void) pname; (void) param; }

void glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
                      GLsizei width, GLsizei height,
                      GLenum format, GLenum type, const GLvoid *pixels)
{ (void) target; (void) level; (void) xoffset; (void) yoffset; (void) width;
  (void) height; (void) format; (void) type; (void) pixels; }

void glTranslatef (GLfloat x, GLfloat y, GLfloat z) { (void) x; (void) y; (void) z; }


void glViewport (GLint x, GLint y, GLsizei width, GLsizei height)
{ (void) x; (void) y; (void) width; (void) height; }
