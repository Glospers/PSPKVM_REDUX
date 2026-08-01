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

/*----------------------------------------------------------------------
 * The read-back corrector
 *
 * Linked with -Wl,--wrap,glReadPixels (psp/Makefile), like the two EGL
 * entry points corrected in m3g_psp_egl.c.
 *
 * m3gcore reads the finished frame out of the back buffer with a hardcoded
 * glReadPixels(..., GL_RGBA, GL_UNSIGNED_BYTE, temp) and converts to the
 * target format on the CPU afterwards (src/m3g_rendercontext.inl:1085) --
 * reasonable against a desktop GL, where ReadPixels converts.  pspgl does
 * not convert: it reads a surface only in the surface's own format, and for
 * any other request it raises GL_INVALID_ENUM and copies NOTHING
 * (pspgl glReadPixels.c) -- silently, because m3gcore only checks GL errors
 * in debug builds.  The back buffer here is 5650, so the engine's read-back
 * has never once executed: it converted its own uninitialised temp buffer
 * into the target, which is where the black frames came from after every
 * other stage of the pipeline had been made to work.
 *
 * So the RGBA8 request is translated: read the pixels in the surface's real
 * format into a buffer of our own, then expand them to the RGBA8 bytes the
 * engine expects.  Two details are deliberate:
 *
 *   - The intermediate buffer is 64-byte aligned.  pspgl picks between a GE
 *     block transfer and a row-by-row CPU copy on the alignment of the
 *     destination, and the GE transfer is the one that is fast on hardware
 *     and the only one PPSSPP materialises into CPU-visible memory.
 *   - It is sized to the largest read m3gcore makes: its own read-back loop
 *     is chunked to at most 16384 bytes of RGBA, i.e. 4096 pixels per call.
 *     Larger requests are split into bands with plain glReadPixels row
 *     semantics, so the wrapper stays correct for any caller.
 *--------------------------------------------------------------------*/

#include <stddef.h>     /* size_t */
#include <stdlib.h>     /* free -- the frame cache below      */
#include <malloc.h>     /* memalign -- ditto                  */
#include <stdio.h>      /* sprintf -- the one-shot dump below */
#include <string.h>     /* strlen/strcat -- ditto             */

/*
 * The one 16-bit read pspgl performs rather than refuses.
 *
 * Its format table (pspgl_texobj.c, __pspgl_texformats) marks
 * (GL_RGB, GL_UNSIGNED_SHORT_5_6_5) as a CONVERTING format -- GL's 5_6_5
 * packing puts red in the high bits, the GE puts it in the low bits -- and
 * glReadPixels refuses any format that is not TF_NATIVE.  The native spelling
 * of the GE's own layout is the _REV type, so that is what is requested, and
 * the expansion below decodes red from the low bits to match.
 */
#ifndef GL_UNSIGNED_SHORT_5_6_5_REV
#define GL_UNSIGNED_SHORT_5_6_5_REV       0x8364
#endif

extern void __real_glReadPixels (GLint x, GLint y,
                                 GLsizei width, GLsizei height,
                                 GLenum format, GLenum type, void *pixels);

/*
 * The whole-frame cache.
 *
 * m3gcore reads a frame back in chunks -- 16384 bytes of RGBA at a time,
 * thirty-four calls for a full screen -- and every single call pays a GE
 * sync: pspgl's aligned path ends in glFinish, and under PPSSPP each one is
 * also a GPU read-back stall.  That is the input lag the 3D era introduced.
 *
 * m3gPspReleaseTarget announces the frame about to be read
 * (m3gPspReadbackHint below); the first chunk that matches it reads the
 * whole frame in ONE native transfer, and the remaining chunks are served
 * from the cache without touching GL at all.
 */
static unsigned short *s_frame;
static int s_frameCapacity;             /* in pixels                        */
static int s_hintX, s_hintY, s_hintW, s_hintH;
static int s_frameValid;

void m3gPspReadbackHint(int x, int y, int width, int height)
{
    s_hintX = x;
    s_hintY = y;
    s_hintW = width;
    s_hintH = height;
    s_frameValid = 0;
}

/*! \brief Expands GE 5650 pixels to the RGBA8 bytes m3gcore expects. */
static void m3gExpand565(const unsigned short *src, unsigned char *dst,
                         int count)
{
    int i;

    for (i = 0; i < count; ++i) {
        unsigned p = src[i];
        unsigned red   = (p >> 11) & 0x1F;
        unsigned green = (p >>  5) & 0x3F;
        unsigned blue  =  p        & 0x1F;

        dst[0] = (unsigned char) ((red   << 3) | (red   >> 2));
        dst[1] = (unsigned char) ((green << 2) | (green >> 4));
        dst[2] = (unsigned char) ((blue  << 3) | (blue  >> 2));
        dst[3] = 0xFF;
        dst += 4;
    }
}

void __wrap_glReadPixels (GLint x, GLint y,
                          GLsizei width, GLsizei height,
                          GLenum format, GLenum type, void *pixels)
{
    /* 4096 pixels: one full chunk of m3gcore's read-back loop.  The
     * fallback for reads outside the hinted frame. */
    static unsigned short s_tmp[4096] __attribute__((aligned(64)));

    int stride, row;

    if (format != GL_RGBA || type != GL_UNSIGNED_BYTE) {
        /* Not the engine's read-back pattern: hand it straight through. */
        __real_glReadPixels(x, y, width, height, format, type, pixels);
        return;
    }

    if (width <= 0 || height <= 0) {
        return;
    }

    /* The frame path: this chunk is part of the read the release announced. */
    if (x == s_hintX && width == s_hintW
        && y >= s_hintY && (y + height) <= (s_hintY + s_hintH)) {

        /* pspgl pads 16-bit rows to the pack alignment of 4 bytes. */
        stride = (s_hintW + 1) & ~1;

        if (!s_frameValid) {
            int need = stride * s_hintH;

            if (need > s_frameCapacity) {
                free(s_frame);
                /* 64-byte aligned so pspgl takes its GE-transfer path --
                 * the fast one, and the only one PPSSPP materialises. */
                s_frame = (unsigned short *) memalign(64, (size_t) need * 2);
                s_frameCapacity = (s_frame != NULL) ? need : 0;
            }
            if (s_frame != NULL) {
                __real_glReadPixels(s_hintX, s_hintY, s_hintW, s_hintH,
                                    GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV,
                                    s_frame);
                s_frameValid = 1;

#if defined(M3G_PSP_READBACK_DUMP)
                /* TEMPORARY -- what did the GE hand back, before any of our
                 * conversion touches it?  White downstream with real values
                 * here indicts the expansion; 0xFFFF here is a depth-buffer
                 * read; zeros here and the transfer did not materialise. */
                {
                    extern void javacall_diag_log(const char *s)
                        __attribute__((weak));
                    extern unsigned int glGetError(void);
                    /* Sampled on the 8th frame, not the 1st: the first frame
                     * of a scene is before anything has animated in, and one
                     * early frame already proved misleading. Five rows by
                     * three columns names which region holds content. */
                    static int calls;
                    if (++calls == 8 && javacall_diag_log != 0) {
                        char line[224];
                        int rows[5], r;
                        rows[0] = 0;
                        rows[1] = s_hintH / 4;
                        rows[2] = s_hintH / 2;
                        rows[3] = (3 * s_hintH) / 4;
                        rows[4] = s_hintH - 1;
                        sprintf(line, "M3G: raw565 err=0x%x", glGetError());
                        for (r = 0; r < 5; ++r) {
                            size_t base = (size_t) rows[r] * stride;
                            sprintf(line + strlen(line),
                                    " r%d=%04x,%04x,%04x", rows[r],
                                    s_frame[base + 8],
                                    s_frame[base + s_hintW / 2],
                                    s_frame[base + s_hintW - 8]);
                        }
                        strcat(line, "\n");
                        javacall_diag_log(line);
                    }
                }
#endif
            }
        }

        if (s_frameValid) {
            for (row = 0; row < height; ++row) {
                m3gExpand565(
                    s_frame + (size_t) (y - s_hintY + row) * (size_t) stride,
                    (unsigned char *) pixels + (size_t) row * (size_t) width * 4,
                    width);
            }
            return;
        }
        /* Allocation failed: fall through to the chunked path. */
    }

    /* The general path: correct for any caller, one GL read per band. */
    stride = (width + 1) & ~1;
    {
        int rowsPerBand = (int) (sizeof(s_tmp) / sizeof(s_tmp[0])) / stride;
        if (rowsPerBand < 1) {
            rowsPerBand = 1;
        }

        for (row = 0; row < height; row += rowsPerBand) {
            int rows = (height - row < rowsPerBand) ? (height - row)
                                                    : rowsPerBand;
            int r;

            __real_glReadPixels(x, y + row, width, rows,
                                GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, s_tmp);

            for (r = 0; r < rows; ++r) {
                m3gExpand565(
                    s_tmp + (size_t) r * (size_t) stride,
                    (unsigned char *) pixels
                        + ((size_t) row + (size_t) r) * (size_t) width * 4,
                    width);
            }
        }
    }
}

/*----------------------------------------------------------------------
 * Culling override
 *
 * Linked with -Wl,--wrap,glEnable, the same mechanism as the other three
 * corrected entry points.
 *
 * The GL-to-GE vertical flip changes the screen-space orientation of every
 * triangle, and with it which faces are "front".  If m3gcore's winding
 * assumption is inverted here, back-face culling removes the entire scene --
 * which looks exactly like geometry never rasterising at all.  With
 * M3G_PSP_NO_CULL defined, face culling can never be enabled, which turns
 * that hypothesis into a one-run experiment: shapes appearing means the
 * winding is inverted (and the real fix is a glFrontFace correction);
 * no change acquits culling entirely.
 *--------------------------------------------------------------------*/

#ifndef GL_CULL_FACE
#define GL_CULL_FACE                      0x0B44
#endif

/*
 * The capability filter -- also wired to -Wl,--wrap,glDisable.
 *
 * m3gcore defensively toggles every capability GL defines, including ones
 * that do not exist on this hardware: multisampling, polygon offset, lights
 * beyond the GE's four, normal rescaling.  pspgl's dispatcher raises
 * GL_INVALID_ENUM for every one of them -- and it does so at a staggering
 * rate (a seventy-megabyte, 1.5-million-line error log accumulated on the
 * memory stick), with two consequences: each logged error is a memory-stick
 * file open/append/close (a large share of the input lag), and the latched
 * error is read by the engine's texture-commit check, which then silently
 * invalidates the texture it was committing (the untextured white scene).
 *
 * The filter forwards exactly the capabilities pspgl's own switch handles
 * (glEnable.c of the pinned source) and swallows the rest as the no-ops
 * they are on this hardware.
 */
static int m3gPspCapSupported(GLenum cap)
{
    switch (cap) {
    case 0x0B60:            /* GL_FOG            */
    case 0x0B50:            /* GL_LIGHTING       */
    case 0x0DE1:            /* GL_TEXTURE_2D     */
    case 0x0B44:            /* GL_CULL_FACE      */
    case 0x0BC0:            /* GL_ALPHA_TEST     */
    case 0x0BE2:            /* GL_BLEND          */
    case 0x0BF2:            /* GL_COLOR_LOGIC_OP */
    case 0x0BD0:            /* GL_DITHER         */
    case 0x0B90:            /* GL_STENCIL_TEST   */
    case 0x0B71:            /* GL_DEPTH_TEST     */
    case 0x4000:            /* GL_LIGHT0..3      */
    case 0x4001:
    case 0x4002:
    case 0x4003:
    case 0x0B20:            /* GL_LINE_SMOOTH    */
    case 0x0B10:            /* GL_POINT_SMOOTH   */
    case 0x0C11:            /* GL_SCISSOR_TEST   */
        return 1;
    default:
        return 0;
    }
}

extern void __real_glEnable (GLenum cap);
extern void __real_glDisable (GLenum cap);

void __wrap_glEnable (GLenum cap)
{
#if defined(M3G_PSP_NO_CULL)
    if (cap == GL_CULL_FACE) {
        return;
    }
#endif
    if (m3gPspCapSupported(cap)) {
        __real_glEnable(cap);
    }
}

void __wrap_glDisable (GLenum cap)
{
    if (m3gPspCapSupported(cap)) {
        __real_glDisable(cap);
    }
}

/*----------------------------------------------------------------------
 * The vertex-stride corrector
 *
 * Linked with -Wl,--wrap,__pspgl_ge_vertex_fmt.
 *
 * The GE has no vertex-stride register: it derives the stride of a vertex
 * stream entirely from the vertex-type bits, laying components out at their
 * natural alignment and padding the struct to the largest component's
 * alignment.  pspgl builds its conversion layout the same way -- and then
 * pads the total to a multiple of 4 (pspgl_varray.c, the trailing ROUNDUP
 * in __pspgl_ge_vertex_fmt).  For any format whose natural size is not a
 * multiple of 4 -- 16-bit positions with byte normals, i.e. every mesh in
 * an .m3g file -- pspgl then writes vertices 16 bytes apart while the GE
 * reads them 14 bytes apart, and every vertex after the first is garbage.
 *
 * All-float formats are naturally 4-aligned, which is why the cube demo and
 * every float probe drew correctly while the engine's scenes never did.
 *
 * The fix recomputes vertex_size the way the GE defines it.  The attribute
 * offsets pspgl computed are already natural and stay untouched.
 *--------------------------------------------------------------------*/

/* Mirror of pspgl_internal.h's vertex_format, offsets verified against the
 * disassembly of the shipped libGL.a (see the note on struct mirroring in
 * m3g_psp_vidmem.c). Only hwformat, vertex_size and the attrib offset/size
 * pairs are read or written. */
struct m3gPspGlAttrib {
    unsigned offset;
    unsigned size;
    void *array;
    void *convert;
};

struct m3gPspGlVertexFormat {
    unsigned hwformat;
    unsigned vertex_size;
    unsigned arrays;
    int nattrib;
    struct m3gPspGlAttrib attribs[5];
};

extern void __real___pspgl_ge_vertex_fmt(void *ctx,
                                         struct m3gPspGlVertexFormat *vfmt);

/*
 * Integral-to-float vertex promotion.
 *
 * The GE reads 8/16-bit vertex components as fractions of 127/32767, so
 * pspgl compensates by folding a scale into the model-view and texture
 * matrices at flush time (MF_ADJUST, pspgl_context.c flush_matrix).  That
 * fold is computed on the VFPU with vmmul -- whose first operand is used
 * TRANSPOSED -- and flush_matrix passes the matrix as the first operand: the
 * flushed result is M-transpose times the adjustment.  For a rotation that
 * inverts it; for a translation it moves the offset into the fourth row,
 * which the GE's 4x3 model/view matrices DISCARD.  Net effect: every draw
 * with integral vertices and a translated model-view collapses to the
 * origin with an inverted rotation -- which described every mesh of every
 * .m3g scene, while all-float draws (the demos, the probes) never touch the
 * adjust path and render perfectly.
 *
 * Rather than re-implement the matrix fold outside the library, the
 * conversion below makes the adjust path unreachable: integral positions
 * and texture coordinates are promoted to float while pspgl interleaves the
 * vertex stream (it is already touching every vertex there), which is
 * exactly the semantic OpenGL defines for integral arrays.  A few hundred
 * extra float stores per frame.
 */

static void m3gCvtShort3ToFloat3(void *to, const void *from, const void *a)
{
    const short *s = (const short *) from;
    float *d = (float *) to;
    (void) a;
    d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
}

static void m3gCvtByte3ToFloat3(void *to, const void *from, const void *a)
{
    const signed char *s = (const signed char *) from;
    float *d = (float *) to;
    (void) a;
    d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
}

static void m3gCvtShort2ToFloat2(void *to, const void *from, const void *a)
{
    const short *s = (const short *) from;
    float *d = (float *) to;
    (void) a;
    d[0] = s[0]; d[1] = s[1];
}

static void m3gCvtByte2ToFloat2(void *to, const void *from, const void *a)
{
    const signed char *s = (const signed char *) from;
    float *d = (float *) to;
    (void) a;
    d[0] = s[0]; d[1] = s[1];
}

void __wrap___pspgl_ge_vertex_fmt(void *ctx,
                                  struct m3gPspGlVertexFormat *vfmt)
{
    /* GE component sizes by type field value: none, byte, short, float. */
    static const unsigned char alignOf[4] = { 1, 1, 2, 4 };

    unsigned hw, off, maxAlign;
    int i, attrIndex;

    __real___pspgl_ge_vertex_fmt(ctx, vfmt);

    if (vfmt->nattrib == 0) {
        return;
    }

    hw = vfmt->hwformat;

    /* Promote integral texcoords and positions to float.  Attribute order in
     * vfmt is fixed by the hardware format: texcoord, weight, colour, normal,
     * position -- present when the matching hwformat field is non-zero.
     * (Weights and normals keep their types: the GE's fractional reading is
     * the semantic both sides already agree on for them.) */
    attrIndex = 0;

    if ((hw & 3) != 0) {                        /* texcoord present */
        unsigned t = hw & 3;
        if (t == 1 || t == 2) {
            struct m3gPspGlAttrib *attr = &vfmt->attribs[attrIndex];
            attr->convert = (void *) ((t == 1) ? m3gCvtByte2ToFloat2
                                               : m3gCvtShort2ToFloat2);
            attr->size = 2 * 4;
            /* The array no longer matches the hardware layout: keep the
             * conversion path honest (native flag, +16 in the array). */
            *((unsigned char *) attr->array + 16) = 0;
            hw = (hw & ~3u) | 3u;
        }
        attrIndex++;
    }
    if (((hw >> 9) & 3) != 0) attrIndex++;      /* weights: untouched */
    if (((hw >> 2) & 7) != 0) attrIndex++;      /* colour:  untouched */
    if (((hw >> 5) & 3) != 0) attrIndex++;      /* normal:  untouched */

    if (((hw >> 7) & 3) != 0) {                 /* position, always last */
        unsigned t = (hw >> 7) & 3;
        if (t == 1 || t == 2) {
            struct m3gPspGlAttrib *attr = &vfmt->attribs[attrIndex];
            attr->convert = (void *) ((t == 1) ? m3gCvtByte3ToFloat3
                                               : m3gCvtShort3ToFloat3);
            attr->size = 3 * 4;
            *((unsigned char *) attr->array + 16) = 0;
            hw = (hw & ~(3u << 7)) | (3u << 7);
        }
    }

    vfmt->hwformat = hw;

    /* Re-lay the vertex out the way the GE defines it: components at their
     * natural alignment, struct padded to the largest component's.  (pspgl's
     * own trailing round-to-4 was already wrong for 14-byte formats; the
     * promotion above changes sizes as well, so everything is recomputed.) */
    off = 0;
    maxAlign = 1;
    for (i = 0; i < vfmt->nattrib; ++i) {
        struct m3gPspGlAttrib *attr = &vfmt->attribs[i];
        unsigned align;

        /* Infer the component alignment from the attribute's place in the
         * (rewritten) format word rather than trusting old offsets. */
        if (i == vfmt->nattrib - 1)      align = alignOf[(hw >> 7) & 3];
        else if (i == 0 && (hw & 3))     align = alignOf[hw & 3];
        else if (attr->size == 4)        align = 4;   /* colour 8888 */
        else                             align = alignOf[(hw >> 5) & 3];

        if (align > maxAlign) {
            maxAlign = align;
        }
        off = (off + align - 1) & ~(align - 1);
        attr->offset = off;
        off += attr->size;
    }
    vfmt->vertex_size = (off + maxAlign - 1) & ~(maxAlign - 1);
}

/*----------------------------------------------------------------------
 * Draw accounting
 *
 * Linked with -Wl,--wrap,glDrawElements and -Wl,--wrap,glDrawArrays.
 *
 * The host harness proves the engine emits draw calls for these scenes; the
 * device screen shows no geometry.  These counters settle, on the device,
 * whether the engine's draws still happen (and the GE loses them) or the
 * engine itself is skipping meshes -- e.g. refusing appearances whose
 * textures did not survive the on-device commit path, which the host's stub
 * GL cannot reproduce because everything trivially succeeds there.
 *
 * Counters are reset and read by the KNI layer around each frame.
 *--------------------------------------------------------------------*/

int m3gPspDrawCalls;
int m3gPspDrawVertices;

extern void __real_glDrawElements (GLenum mode, GLsizei count, GLenum type,
                                   const void *indices);
extern void __real_glDrawArrays (GLenum mode, GLint first, GLsizei count);

void __wrap_glDrawElements (GLenum mode, GLsizei count, GLenum type,
                            const void *indices)
{
    m3gPspDrawCalls++;
    m3gPspDrawVertices += (int) count;
    __real_glDrawElements(mode, count, type, indices);

    /* TEMPORARY -- what state did the first engine draws actually run
     * under?  The vertex-type word says whether a colour array was in the
     * stream (bits 2-4), the enables whether texturing/lighting were on,
     * and the texture registers what was bound.  One line for each of the
     * first eight draws of a session. */
    {
        extern void *__pspgl_curctx;
        extern void javacall_diag_log(const char *s) __attribute__((weak));
        static int logged;

        if (logged < 8 && __pspgl_curctx != 0 && javacall_diag_log != 0) {
            const unsigned int *reg =
                (const unsigned int *) ((char *) __pspgl_curctx + 8);
            char line[200];
            logged++;
            sprintf(line,
                    "M3G: draw%d n=%d vt=%06x texEna=%d lightEna=%d "
                    "tbp=%06x tfmt=%06x tenv=%06x\n",
                    logged, (int) count,
                    reg[0x12] & 0xFFFFFF,
                    reg[0x1E] & 1, reg[0x17] & 1,
                    reg[0xA0] & 0xFFFFFF, reg[0xC3] & 0xFFFFFF,
                    reg[0xC9] & 0xFFFFFF);
            javacall_diag_log(line);
        }
    }
}

void __wrap_glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
    m3gPspDrawCalls++;
    m3gPspDrawVertices += (int) count;
    __real_glDrawArrays(mode, first, count);
}

/*----------------------------------------------------------------------
 * GL error visibility
 *
 * Linked with -Wl,--wrap,glGetError.  m3gcore polls glGetError around its
 * texture uploads and silently invalidates a texture whose upload errored --
 * which disables the texture unit for every draw that uses it.  The skybox
 * texture (2x2 luminance-alpha plus a 1x1 mip level) is refused on the
 * device while the game's own RGBA textures work, and this says what error
 * pspgl actually raised, from where.
 *--------------------------------------------------------------------*/

extern unsigned int __real_glGetError (void);

unsigned int __wrap_glGetError (void)
{
    extern void javacall_diag_log(const char *s) __attribute__((weak));
    static int logged;

    unsigned int err = __real_glGetError();

    if (err != 0 && logged < 6 && javacall_diag_log != 0) {
        char line[96];
        logged++;
        /* The return address names the engine call site that polled the
         * error; psp-addr2line -e pspkvm.elf (address - 0x08804000 + the
         * link base) resolves it to the exact source line. */
        sprintf(line, "M3G: glError 0x%x ra=%p\n",
                err, __builtin_return_address(0));
        javacall_diag_log(line);
    }

    /*
     * INVALID_ENUM immunity.  On this stack every INVALID_ENUM seen so far
     * has meant "a feature this hardware does not have was toggled" -- a
     * harmless no-op -- but m3gcore's texture-commit check treats any latched
     * error as a failed upload and silently invalidates the texture, which
     * blanked every mipmapped texture in every scene.  Known chatty sources
     * are filtered at their entry points above; this is the backstop that
     * keeps the next unknown one from costing another debugging session.
     * Real failures (out of memory in particular) still pass through.
     */
    if (err == 0x0500 /* GL_INVALID_ENUM */) {
        return 0;
    }
    return err;
}

/*----------------------------------------------------------------------
 * Light-model filter -- -Wl,--wrap,glLightModelf / glLightModelfv.
 *
 * m3gcore sets GL_LIGHT_MODEL_TWO_SIDE around every lit draw; pspgl's
 * light-model switch knows only AMBIENT and COLOR_CONTROL and raises
 * GL_INVALID_ENUM (nineteen hundred times a run, each one a memory-stick
 * log write inside pspgl).  The GE has no two-sided lighting, so the
 * correct translation is a no-op.
 *--------------------------------------------------------------------*/

#ifndef GL_LIGHT_MODEL_TWO_SIDE
#define GL_LIGHT_MODEL_TWO_SIDE           0x0B52
#endif

extern void __real_glLightModelf (GLenum pname, GLfloat param);
extern void __real_glLightModelfv (GLenum pname, const GLfloat *params);

void __wrap_glLightModelf (GLenum pname, GLfloat param)
{
    if (pname != GL_LIGHT_MODEL_TWO_SIDE) {
        __real_glLightModelf(pname, param);
    }
}

void __wrap_glLightModelfv (GLenum pname, const GLfloat *params)
{
    if (pname != GL_LIGHT_MODEL_TWO_SIDE) {
        __real_glLightModelfv(pname, params);
    }
}
