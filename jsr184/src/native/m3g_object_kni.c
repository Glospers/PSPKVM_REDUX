/*
 * m3g_object_kni.c -- the natives behind the javax.microedition.m3g
 * constructors and mutators.
 *
 * Until now only objects that came out of Loader had an engine object behind
 * them; anything a MIDlet built with `new Camera()`, `new Mesh(...)` and the
 * rest was a Java-side placeholder with Object3D.handle == 0, so the renderer
 * had nothing to draw and m3gRenderNode refused the frame outright (it will
 * not draw while the context has no camera --
 * m3gcore/src/m3g_rendercontext.c:1812).  This file closes that gap: every
 * public constructor that matters for drawing now calls the matching
 * m3gCreate*, and every setter that rendering depends on forwards its value
 * into the engine, which becomes the source of truth.
 *
 * Conventions, all inherited from m3g_loader_kni.c / m3g_graphics3d_kni.c:
 *
 *   - phoneME binds natives statically from the class and method name alone
 *     (cldc/src/vm/share/ROM/SourceObjectWriter.cpp:648), so the spelling of
 *     the symbols below is the whole of the registration.  Consequently no two
 *     native methods of the same Java class may share a name -- overloads get
 *     distinct nFoo names (VertexArray.nSetByte / nSetShort).
 *   - Every native is declared static on the Java side, so KNI parameter
 *     index 1 is the first argument.
 *   - Handles cross the boundary as plain ints; m3gcore keeps object pointers
 *     in M3Guint fields and asserts sizeof(M3Guint) >= sizeof(void*)
 *     (m3gcore/inc/m3g_defs.h:609).
 *
 * REFERENCE DISCIPLINE
 *
 * m3gCreate* hands back an object with a reference count of zero
 * (m3gcore/src/m3g_object.c:55), so the wrapper takes one reference and holds
 * it -- the same rule m3g/src/m3g_psp_loader.c applies to loaded roots.  CLDC
 * has no finalization, so nothing drops it again; engine objects a MIDlet
 * discards leak until the VM exits.  That is a known and accepted limitation
 * at this stage.  The one place a reference *is* dropped is Object3D.adopt(),
 * which releases the object a public constructor just made when the instance
 * turns out to be a wrapper for something the loader already built.
 *
 * RAW ARRAY POINTERS
 *
 * SNI_GetRawArrayPointer hands out an address inside the Java heap, which is
 * only safe while nothing can move it.  Nothing here can: every m3g* call
 * allocates from the M3G arena (m3g/src/m3g_psp_arena.c) and never re-enters
 * the VM, so no garbage collection can happen between taking the pointer and
 * the engine finishing with it.  That is what lets a Java int[] of handles be
 * passed straight to m3gCreateMesh as an M3GIndexBuffer array -- both are
 * arrays of 4-byte words with identical layout.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include <stddef.h>     /* NULL -- kni.h does not pull in a libc header */
#include <stdio.h>      /* sprintf, for the diagnostic line below       */

#include <kni.h>
#include <sni.h>

#include "M3G/m3g_psp.h"

/*----------------------------------------------------------------------
 * Diagnostics
 *
 * Same sink as the other two natives files, ms0:/pspkvm_vm.log, weak so a
 * build without docker/patches/0043 -- and the romgen host tool, which has no
 * javacall at all -- still links.
 *
 * Only *failures* are traced.  A scene is built out of hundreds of these calls
 * and every log line is an open/write/close on the memory stick, so tracing
 * successes would be the thing being measured.  A failed creation, on the
 * other hand, is almost always the arena running out, which is worth the
 * write; the counters go on the line so the size can be retuned without a
 * second run.
 *--------------------------------------------------------------------*/

extern void javacall_diag_log(const char *s) __attribute__((weak));

#define M3G_FAIL_TRACE_BUDGET 12
static int s_failTraceLeft = M3G_FAIL_TRACE_BUDGET;

static void m3gReportFailure(const char *what)
{
    M3GPspArenaStats st;
    char line[160];

    if (javacall_diag_log == 0 || s_failTraceLeft <= 0) {
        return;
    }
    --s_failTraceLeft;

    m3gPspArenaGetStats(&st);
    sprintf(line, "M3G: create %s FAILED arena used=%d peak=%d cap=%d fail=%d\n",
            what, (int) st.used, (int) st.peak,
            (int) st.capacity, (int) st.failures);
    javacall_diag_log(line);
}

/*----------------------------------------------------------------------
 * Shared helpers
 *--------------------------------------------------------------------*/

/*! \brief Takes the wrapper's reference on a freshly created object. */
static jint m3gOwn(const char *what, void *object)
{
    if (object == NULL) {
        m3gReportFailure(what);
        return 0;
    }
    m3gAddRef((M3GObject) object);
    return (jint) object;
}

#define M3G_MATRIX_FLOATS 16

/*!
 * \brief Reads a JSR-184 Transform (16 row-major floats) into an M3GMatrix.
 *
 * \return M3G_TRUE if \a out was filled, M3G_FALSE for a null or short array,
 *         in which case the caller should treat the argument as absent.
 */
static M3Gbool m3gFetchMatrix(jobject array, M3GMatrix *out)
{
    const M3Gfloat *src;

    if (KNI_IsNullHandle(array)
        || KNI_GetArrayLength(array) < M3G_MATRIX_FLOATS) {
        return M3G_FALSE;
    }
    src = (const M3Gfloat *) SNI_GetRawArrayPointer(array);
    if (src == NULL) {
        return M3G_FALSE;
    }
    /* JSR-184 hands matrices out row-major, which is what m3gSetMatrixRows
     * expects; the engine stores columns internally. */
    m3gSetMatrixRows(out, src);
    return M3G_TRUE;
}

/*! \brief Writes an M3GMatrix back into a 16-element Java float[]. */
static void m3gStoreMatrix(jobject array, const M3GMatrix *in)
{
    M3Gfloat *dst;

    if (KNI_IsNullHandle(array)
        || KNI_GetArrayLength(array) < M3G_MATRIX_FLOATS) {
        return;
    }
    dst = (M3Gfloat *) SNI_GetRawArrayPointer(array);
    if (dst != NULL) {
        m3gGetMatrixRows(in, dst);
    }
}

/*----------------------------------------------------------------------
 * Object3D
 *--------------------------------------------------------------------*/

/*
 * private static native void nDeleteRef(int handle);
 *
 * Drops the wrapper's reference.  Only Object3D.adopt() uses it, to throw away
 * the object a public constructor built when the instance is really a wrapper
 * for one the loader already made.
 */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Object3D_nDeleteRef()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gDeleteRef((M3GObject) handle);
    }
    KNI_ReturnVoid();
}

/*
 * private static native void nSetUserID(int handle, int userID);
 */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Object3D_nSetUserID()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetUserID((M3GObject) handle, KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * Transformable
 *
 * These decide where anything appears, so they are the ones that matter most
 * after the objects themselves exist.  Texture2D is a Transformable too, which
 * is why the texture matrix comes for free.
 *--------------------------------------------------------------------*/

/* private static native void nSetTransform(int handle, float[] matrix); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Transformable_nSetTransform()
{
    jint handle = KNI_GetParameterAsInt(1);
    M3GMatrix matrix;
    M3Gbool have;

    if (handle == 0) {
        KNI_ReturnVoid();
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(array);
    KNI_GetParameterAsObject(2, array);
    have = m3gFetchMatrix(array, &matrix);
    KNI_EndHandles();

    if (!have) {
        m3gIdentityMatrix(&matrix);
    }
    m3gSetTransform((M3GTransformable) handle, &matrix);
    KNI_ReturnVoid();
}

/* private static native void nGetTransform(int handle, float[] matrix); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Transformable_nGetTransform()
{
    jint handle = KNI_GetParameterAsInt(1);
    M3GMatrix matrix;

    if (handle == 0) {
        KNI_ReturnVoid();
    }
    m3gGetTransform((M3GTransformable) handle, &matrix);

    KNI_StartHandles(1);
    KNI_DeclareHandle(array);
    KNI_GetParameterAsObject(2, array);
    m3gStoreMatrix(array, &matrix);
    KNI_EndHandles();

    KNI_ReturnVoid();
}

/* private static native void nGetCompositeTransform(int handle, float[] m); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Transformable_nGetCompositeTransform()
{
    jint handle = KNI_GetParameterAsInt(1);
    M3GMatrix matrix;

    if (handle == 0) {
        KNI_ReturnVoid();
    }
    m3gGetCompositeTransform((M3GTransformable) handle, &matrix);

    KNI_StartHandles(1);
    KNI_DeclareHandle(array);
    KNI_GetParameterAsObject(2, array);
    m3gStoreMatrix(array, &matrix);
    KNI_EndHandles();

    KNI_ReturnVoid();
}

/* private static native void nSetTranslation(int h, float x, float y, float z); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Transformable_nSetTranslation()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetTranslation((M3GTransformable) handle,
                          (M3Gfloat) KNI_GetParameterAsFloat(2),
                          (M3Gfloat) KNI_GetParameterAsFloat(3),
                          (M3Gfloat) KNI_GetParameterAsFloat(4));
    }
    KNI_ReturnVoid();
}

/* private static native void nTranslate(int h, float x, float y, float z); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Transformable_nTranslate()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gTranslate((M3GTransformable) handle,
                     (M3Gfloat) KNI_GetParameterAsFloat(2),
                     (M3Gfloat) KNI_GetParameterAsFloat(3),
                     (M3Gfloat) KNI_GetParameterAsFloat(4));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetScale(int h, float x, float y, float z); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Transformable_nSetScale()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetScale((M3GTransformable) handle,
                    (M3Gfloat) KNI_GetParameterAsFloat(2),
                    (M3Gfloat) KNI_GetParameterAsFloat(3),
                    (M3Gfloat) KNI_GetParameterAsFloat(4));
    }
    KNI_ReturnVoid();
}

/* private static native void nScale(int h, float x, float y, float z); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Transformable_nScale()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gScale((M3GTransformable) handle,
                 (M3Gfloat) KNI_GetParameterAsFloat(2),
                 (M3Gfloat) KNI_GetParameterAsFloat(3),
                 (M3Gfloat) KNI_GetParameterAsFloat(4));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetOrientation(int h, float a, float x, float y, float z); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Transformable_nSetOrientation()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetOrientation((M3GTransformable) handle,
                          (M3Gfloat) KNI_GetParameterAsFloat(2),
                          (M3Gfloat) KNI_GetParameterAsFloat(3),
                          (M3Gfloat) KNI_GetParameterAsFloat(4),
                          (M3Gfloat) KNI_GetParameterAsFloat(5));
    }
    KNI_ReturnVoid();
}

/* private static native void nPreRotate(int h, float a, float x, float y, float z); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Transformable_nPreRotate()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gPreRotate((M3GTransformable) handle,
                     (M3Gfloat) KNI_GetParameterAsFloat(2),
                     (M3Gfloat) KNI_GetParameterAsFloat(3),
                     (M3Gfloat) KNI_GetParameterAsFloat(4),
                     (M3Gfloat) KNI_GetParameterAsFloat(5));
    }
    KNI_ReturnVoid();
}

/* private static native void nPostRotate(int h, float a, float x, float y, float z); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Transformable_nPostRotate()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gPostRotate((M3GTransformable) handle,
                      (M3Gfloat) KNI_GetParameterAsFloat(2),
                      (M3Gfloat) KNI_GetParameterAsFloat(3),
                      (M3Gfloat) KNI_GetParameterAsFloat(4),
                      (M3Gfloat) KNI_GetParameterAsFloat(5));
    }
    KNI_ReturnVoid();
}

/*
 * The three small vector getters.  They hand the engine the address of the
 * caller's float[] directly, which is safe for the reason given at the top of
 * this file: none of these can move the Java heap.
 *
 * private static native void nGetTranslation(int handle, float[] out);
 * private static native void nGetScale(int handle, float[] out);
 * private static native void nGetOrientation(int handle, float[] out);
 */

static M3Gfloat *m3gVectorOut(jobject array, jint minimum)
{
    if (KNI_IsNullHandle(array) || KNI_GetArrayLength(array) < minimum) {
        return NULL;
    }
    return (M3Gfloat *) SNI_GetRawArrayPointer(array);
}

KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Transformable_nGetTranslation()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle == 0) {
        KNI_ReturnVoid();
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(array);
    KNI_GetParameterAsObject(2, array);
    {
        M3Gfloat *out = m3gVectorOut(array, 3);
        if (out != NULL) {
            m3gGetTranslation((M3GTransformable) handle, out);
        }
    }
    KNI_EndHandles();

    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Transformable_nGetScale()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle == 0) {
        KNI_ReturnVoid();
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(array);
    KNI_GetParameterAsObject(2, array);
    {
        M3Gfloat *out = m3gVectorOut(array, 3);
        if (out != NULL) {
            m3gGetScale((M3GTransformable) handle, out);
        }
    }
    KNI_EndHandles();

    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Transformable_nGetOrientation()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle == 0) {
        KNI_ReturnVoid();
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(array);
    KNI_GetParameterAsObject(2, array);
    {
        M3Gfloat *out = m3gVectorOut(array, 4);
        if (out != NULL) {
            m3gGetOrientation((M3GTransformable) handle, out);
        }
    }
    KNI_EndHandles();

    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * Node
 *--------------------------------------------------------------------*/

/* private static native void nSetAlphaFactor(int handle, float alpha); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Node_nSetAlphaFactor()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetAlphaFactor((M3GNode) handle,
                          (M3Gfloat) KNI_GetParameterAsFloat(2));
    }
    KNI_ReturnVoid();
}

/*
 * private static native void nEnable(int handle, int which, int enable);
 *
 * `which` is M3G_SETGET_RENDERING (0) or M3G_SETGET_PICKING (1).
 */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Node_nEnable()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gEnable((M3GNode) handle,
                  KNI_GetParameterAsInt(2),
                  (M3Gbool) (KNI_GetParameterAsInt(3) ? M3G_TRUE : M3G_FALSE));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetScope(int handle, int scope); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Node_nSetScope()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetScope((M3GNode) handle, KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native boolean nGetTransformTo(int h, int target, float[] m); */
KNIEXPORT KNI_RETURNTYPE_BOOLEAN
Java_javax_microedition_m3g_Node_nGetTransformTo()
{
    jint handle = KNI_GetParameterAsInt(1);
    jint target = KNI_GetParameterAsInt(2);
    M3GMatrix matrix;
    M3Gbool ok;

    if (handle == 0 || target == 0) {
        KNI_ReturnBoolean(KNI_FALSE);
    }

    ok = m3gGetTransformTo((M3GNode) handle, (M3GNode) target, &matrix);
    if (ok) {
        KNI_StartHandles(1);
        KNI_DeclareHandle(array);
        KNI_GetParameterAsObject(3, array);
        m3gStoreMatrix(array, &matrix);
        KNI_EndHandles();
    }
    KNI_ReturnBoolean(ok ? KNI_TRUE : KNI_FALSE);
}

/* private static native void nSetAlignment(int h, int zRef, int zT, int yRef, int yT); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Node_nSetAlignment()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetAlignment((M3GNode) handle,
                        (M3GNode) KNI_GetParameterAsInt(2),
                        KNI_GetParameterAsInt(3),
                        (M3GNode) KNI_GetParameterAsInt(4),
                        KNI_GetParameterAsInt(5));
    }
    KNI_ReturnVoid();
}

/* private static native void nAlign(int handle, int reference); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Node_nAlign()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gAlignNode((M3GNode) handle, (M3GNode) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * Group
 *--------------------------------------------------------------------*/

/* private static native int nCreate(); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Group_nCreate()
{
    KNI_ReturnInt(m3gOwn("Group", m3gCreateGroup(m3gPspGetInterface())));
}

/* private static native void nAddChild(int group, int child); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Group_nAddChild()
{
    jint group = KNI_GetParameterAsInt(1);
    jint child = KNI_GetParameterAsInt(2);

    if (group != 0 && child != 0) {
        m3gAddChild((M3GGroup) group, (M3GNode) child);
    }
    KNI_ReturnVoid();
}

/* private static native void nRemoveChild(int group, int child); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Group_nRemoveChild()
{
    jint group = KNI_GetParameterAsInt(1);
    jint child = KNI_GetParameterAsInt(2);

    if (group != 0 && child != 0) {
        m3gRemoveChild((M3GGroup) group, (M3GNode) child);
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * World
 *--------------------------------------------------------------------*/

/* private static native int nCreate(); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_World_nCreate()
{
    KNI_ReturnInt(m3gOwn("World", m3gCreateWorld(m3gPspGetInterface())));
}

/* private static native void nSetActiveCamera(int world, int camera); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_World_nSetActiveCamera()
{
    jint world = KNI_GetParameterAsInt(1);

    if (world != 0) {
        m3gSetActiveCamera((M3GWorld) world,
                           (M3GCamera) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetBackground(int world, int background); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_World_nSetBackground()
{
    jint world = KNI_GetParameterAsInt(1);

    if (world != 0) {
        m3gSetBackground((M3GWorld) world,
                         (M3GBackground) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * Camera
 *--------------------------------------------------------------------*/

/* private static native int nCreate(); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Camera_nCreate()
{
    KNI_ReturnInt(m3gOwn("Camera", m3gCreateCamera(m3gPspGetInterface())));
}

/* private static native void nSetPerspective(int h, float fovy, float ar,
 *                                            float near, float far); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Camera_nSetPerspective()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetPerspective((M3GCamera) handle,
                          (M3Gfloat) KNI_GetParameterAsFloat(2),
                          (M3Gfloat) KNI_GetParameterAsFloat(3),
                          (M3Gfloat) KNI_GetParameterAsFloat(4),
                          (M3Gfloat) KNI_GetParameterAsFloat(5));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetParallel(int h, float height, float ar,
 *                                         float near, float far); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Camera_nSetParallel()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetParallel((M3GCamera) handle,
                       (M3Gfloat) KNI_GetParameterAsFloat(2),
                       (M3Gfloat) KNI_GetParameterAsFloat(3),
                       (M3Gfloat) KNI_GetParameterAsFloat(4),
                       (M3Gfloat) KNI_GetParameterAsFloat(5));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetGeneric(int handle, float[] matrix); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Camera_nSetGeneric()
{
    jint handle = KNI_GetParameterAsInt(1);
    M3GMatrix matrix;
    M3Gbool have;

    if (handle == 0) {
        KNI_ReturnVoid();
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(array);
    KNI_GetParameterAsObject(2, array);
    have = m3gFetchMatrix(array, &matrix);
    KNI_EndHandles();

    if (have) {
        m3gSetProjectionMatrix((M3GCamera) handle, &matrix);
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * Light
 *--------------------------------------------------------------------*/

/* private static native int nCreate(); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Light_nCreate()
{
    KNI_ReturnInt(m3gOwn("Light", m3gCreateLight(m3gPspGetInterface())));
}

/* private static native void nSetMode(int handle, int mode); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Light_nSetMode()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetLightMode((M3GLight) handle, KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetColor(int handle, int rgb); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Light_nSetColor()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetLightColor((M3GLight) handle,
                         (M3Guint) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetIntensity(int handle, float intensity); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Light_nSetIntensity()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetIntensity((M3GLight) handle,
                        (M3Gfloat) KNI_GetParameterAsFloat(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetSpotAngle(int handle, float angle); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Light_nSetSpotAngle()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetSpotAngle((M3GLight) handle,
                        (M3Gfloat) KNI_GetParameterAsFloat(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetSpotExponent(int handle, float exponent); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Light_nSetSpotExponent()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetSpotExponent((M3GLight) handle,
                           (M3Gfloat) KNI_GetParameterAsFloat(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetAttenuation(int h, float c, float l, float q); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Light_nSetAttenuation()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetAttenuation((M3GLight) handle,
                          (M3Gfloat) KNI_GetParameterAsFloat(2),
                          (M3Gfloat) KNI_GetParameterAsFloat(3),
                          (M3Gfloat) KNI_GetParameterAsFloat(4));
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * Background
 *--------------------------------------------------------------------*/

/* private static native int nCreate(); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Background_nCreate()
{
    KNI_ReturnInt(m3gOwn("Background",
                         m3gCreateBackground(m3gPspGetInterface())));
}

/* private static native void nSetColor(int handle, int argb); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Background_nSetColor()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetBgColor((M3GBackground) handle,
                      (M3Guint) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetImage(int handle, int image); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Background_nSetImage()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetBgImage((M3GBackground) handle,
                      (M3GImage) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetImageMode(int handle, int modeX, int modeY); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Background_nSetImageMode()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetBgMode((M3GBackground) handle,
                     KNI_GetParameterAsInt(2),
                     KNI_GetParameterAsInt(3));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetCrop(int h, int x, int y, int w, int height); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Background_nSetCrop()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetBgCrop((M3GBackground) handle,
                     KNI_GetParameterAsInt(2),
                     KNI_GetParameterAsInt(3),
                     KNI_GetParameterAsInt(4),
                     KNI_GetParameterAsInt(5));
    }
    KNI_ReturnVoid();
}

/*
 * private static native void nSetEnable(int handle, int which, int enable);
 *
 * `which` is M3G_SETGET_COLORCLEAR (0) or M3G_SETGET_DEPTHCLEAR (1).
 */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Background_nSetEnable()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetBgEnable((M3GBackground) handle,
                       KNI_GetParameterAsInt(2),
                       (M3Gbool) (KNI_GetParameterAsInt(3) ? M3G_TRUE
                                                           : M3G_FALSE));
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * Fog
 *--------------------------------------------------------------------*/

/* private static native int nCreate(); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Fog_nCreate()
{
    KNI_ReturnInt(m3gOwn("Fog", m3gCreateFog(m3gPspGetInterface())));
}

/* private static native void nSetMode(int handle, int mode); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Fog_nSetMode()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetFogMode((M3GFog) handle, KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetColor(int handle, int rgb); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Fog_nSetColor()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetFogColor((M3GFog) handle, (M3Guint) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetDensity(int handle, float density); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Fog_nSetDensity()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetFogDensity((M3GFog) handle,
                         (M3Gfloat) KNI_GetParameterAsFloat(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetLinear(int handle, float near, float far); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Fog_nSetLinear()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetFogLinear((M3GFog) handle,
                        (M3Gfloat) KNI_GetParameterAsFloat(2),
                        (M3Gfloat) KNI_GetParameterAsFloat(3));
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * Material
 *--------------------------------------------------------------------*/

/* private static native int nCreate(); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Material_nCreate()
{
    KNI_ReturnInt(m3gOwn("Material", m3gCreateMaterial(m3gPspGetInterface())));
}

/* private static native void nSetColor(int handle, int target, int argb); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Material_nSetColor()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetColor((M3GMaterial) handle,
                    (M3Genum) KNI_GetParameterAsInt(2),
                    (M3Guint) KNI_GetParameterAsInt(3));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetShininess(int handle, float shininess); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Material_nSetShininess()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetShininess((M3GMaterial) handle,
                        (M3Gfloat) KNI_GetParameterAsFloat(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetVertexColorTracking(int handle, int enable); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Material_nSetVertexColorTracking()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetVertexColorTrackingEnable(
            (M3GMaterial) handle,
            (M3Gbool) (KNI_GetParameterAsInt(2) ? M3G_TRUE : M3G_FALSE));
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * PolygonMode
 *--------------------------------------------------------------------*/

/* private static native int nCreate(); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_PolygonMode_nCreate()
{
    KNI_ReturnInt(m3gOwn("PolygonMode",
                         m3gCreatePolygonMode(m3gPspGetInterface())));
}

/* private static native void nSetCulling(int handle, int mode); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_PolygonMode_nSetCulling()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetCulling((M3GPolygonMode) handle, KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetShading(int handle, int mode); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_PolygonMode_nSetShading()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetShading((M3GPolygonMode) handle, KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetWinding(int handle, int mode); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_PolygonMode_nSetWinding()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetWinding((M3GPolygonMode) handle, KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetTwoSidedLighting(int handle, int enable); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_PolygonMode_nSetTwoSidedLighting()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetTwoSidedLightingEnable(
            (M3GPolygonMode) handle,
            (M3Gbool) (KNI_GetParameterAsInt(2) ? M3G_TRUE : M3G_FALSE));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetLocalCameraLighting(int handle, int enable); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_PolygonMode_nSetLocalCameraLighting()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetLocalCameraLightingEnable(
            (M3GPolygonMode) handle,
            (M3Gbool) (KNI_GetParameterAsInt(2) ? M3G_TRUE : M3G_FALSE));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetPerspectiveCorrection(int handle, int enable); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_PolygonMode_nSetPerspectiveCorrection()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetPerspectiveCorrectionEnable(
            (M3GPolygonMode) handle,
            (M3Gbool) (KNI_GetParameterAsInt(2) ? M3G_TRUE : M3G_FALSE));
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * CompositingMode
 *--------------------------------------------------------------------*/

/* private static native int nCreate(); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_CompositingMode_nCreate()
{
    KNI_ReturnInt(m3gOwn("CompositingMode",
                         m3gCreateCompositingMode(m3gPspGetInterface())));
}

/* private static native void nSetBlending(int handle, int mode); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_CompositingMode_nSetBlending()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetBlending((M3GCompositingMode) handle, KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetAlphaThreshold(int handle, float threshold); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_CompositingMode_nSetAlphaThreshold()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetAlphaThreshold((M3GCompositingMode) handle,
                             (M3Gfloat) KNI_GetParameterAsFloat(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetDepthOffset(int h, float factor, float units); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_CompositingMode_nSetDepthOffset()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetDepthOffset((M3GCompositingMode) handle,
                          (M3Gfloat) KNI_GetParameterAsFloat(2),
                          (M3Gfloat) KNI_GetParameterAsFloat(3));
    }
    KNI_ReturnVoid();
}

/* private static native void nEnableDepthTest(int handle, int enable); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_CompositingMode_nEnableDepthTest()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gEnableDepthTest(
            (M3GCompositingMode) handle,
            (M3Gbool) (KNI_GetParameterAsInt(2) ? M3G_TRUE : M3G_FALSE));
    }
    KNI_ReturnVoid();
}

/* private static native void nEnableDepthWrite(int handle, int enable); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_CompositingMode_nEnableDepthWrite()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gEnableDepthWrite(
            (M3GCompositingMode) handle,
            (M3Gbool) (KNI_GetParameterAsInt(2) ? M3G_TRUE : M3G_FALSE));
    }
    KNI_ReturnVoid();
}

/* private static native void nEnableColorWrite(int handle, int enable); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_CompositingMode_nEnableColorWrite()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gEnableColorWrite(
            (M3GCompositingMode) handle,
            (M3Gbool) (KNI_GetParameterAsInt(2) ? M3G_TRUE : M3G_FALSE));
    }
    KNI_ReturnVoid();
}

/* private static native void nEnableAlphaWrite(int handle, int enable); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_CompositingMode_nEnableAlphaWrite()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetAlphaWriteEnable(
            (M3GCompositingMode) handle,
            (M3Gbool) (KNI_GetParameterAsInt(2) ? M3G_TRUE : M3G_FALSE));
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * Appearance
 *--------------------------------------------------------------------*/

/* private static native int nCreate(); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Appearance_nCreate()
{
    KNI_ReturnInt(m3gOwn("Appearance",
                         m3gCreateAppearance(m3gPspGetInterface())));
}

/* private static native void nSetLayer(int handle, int layer); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Appearance_nSetLayer()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetLayer((M3GAppearance) handle, KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetMaterial(int handle, int material); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Appearance_nSetMaterial()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetMaterial((M3GAppearance) handle,
                       (M3GMaterial) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetPolygonMode(int handle, int polygonMode); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Appearance_nSetPolygonMode()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetPolygonMode((M3GAppearance) handle,
                          (M3GPolygonMode) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetCompositingMode(int handle, int mode); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Appearance_nSetCompositingMode()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetCompositingMode((M3GAppearance) handle,
                              (M3GCompositingMode) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetFog(int handle, int fog); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Appearance_nSetFog()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetFog((M3GAppearance) handle, (M3GFog) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetTexture(int handle, int unit, int texture); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Appearance_nSetTexture()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetTexture((M3GAppearance) handle,
                      KNI_GetParameterAsInt(2),
                      (M3GTexture) KNI_GetParameterAsInt(3));
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * Texture2D
 *--------------------------------------------------------------------*/

/* private static native int nCreate(int image); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Texture2D_nCreate()
{
    jint image = KNI_GetParameterAsInt(1);

    if (image == 0) {
        KNI_ReturnInt(0);
    }
    KNI_ReturnInt(m3gOwn("Texture2D",
                         m3gCreateTexture(m3gPspGetInterface(),
                                          (M3GImage) image)));
}

/* private static native void nSetImage(int handle, int image); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Texture2D_nSetImage()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetTextureImage((M3GTexture) handle,
                           (M3GImage) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetFiltering(int h, int level, int image); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Texture2D_nSetFiltering()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetFiltering((M3GTexture) handle,
                        KNI_GetParameterAsInt(2),
                        KNI_GetParameterAsInt(3));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetWrapping(int handle, int wrapS, int wrapT); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Texture2D_nSetWrapping()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetWrapping((M3GTexture) handle,
                       KNI_GetParameterAsInt(2),
                       KNI_GetParameterAsInt(3));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetBlending(int handle, int func); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Texture2D_nSetBlending()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gTextureSetBlending((M3GTexture) handle, KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetBlendColor(int handle, int rgb); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Texture2D_nSetBlendColor()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetBlendColor((M3GTexture) handle,
                         (M3Guint) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * Image2D
 *--------------------------------------------------------------------*/

/*
 * private static native int nCreate(int format, int width, int height,
 *                                   int flags);
 *
 * `flags` is the M3G_STATIC / M3G_DYNAMIC / M3G_PALETTED bitmask of
 * m3g/inc/M3G/m3g_core.h:403-406; the Java side picks it from which
 * constructor was used.
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Image2D_nCreate()
{
    jint format = KNI_GetParameterAsInt(1);
    jint width  = KNI_GetParameterAsInt(2);
    jint height = KNI_GetParameterAsInt(3);
    jint flags  = KNI_GetParameterAsInt(4);

    KNI_ReturnInt(m3gOwn("Image2D",
                         m3gCreateImage(m3gPspGetInterface(),
                                        (M3GImageFormat) format,
                                        width, height,
                                        (M3Gbitmask) flags)));
}

/* private static native void nSetImage(int handle, byte[] pixels); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Image2D_nSetImage()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle == 0) {
        KNI_ReturnVoid();
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(pixels);
    KNI_GetParameterAsObject(2, pixels);
    if (!KNI_IsNullHandle(pixels)) {
        const void *src = SNI_GetRawArrayPointer(pixels);
        if (src != NULL) {
            m3gSetImage((M3GImage) handle, src);
        }
    }
    KNI_EndHandles();

    KNI_ReturnVoid();
}

/* private static native void nSetPalette(int handle, int length, byte[] p); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Image2D_nSetPalette()
{
    jint handle = KNI_GetParameterAsInt(1);
    jint length = KNI_GetParameterAsInt(2);

    if (handle == 0) {
        KNI_ReturnVoid();
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(palette);
    KNI_GetParameterAsObject(3, palette);
    if (!KNI_IsNullHandle(palette)) {
        const void *src = SNI_GetRawArrayPointer(palette);
        if (src != NULL) {
            m3gSetImagePalette((M3GImage) handle, length, src);
        }
    }
    KNI_EndHandles();

    KNI_ReturnVoid();
}

/*
 * private static native void nSetSubImage(int handle, int x, int y,
 *                                         int width, int height,
 *                                         byte[] pixels);
 */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Image2D_nSetSubImage()
{
    jint handle = KNI_GetParameterAsInt(1);
    jint x      = KNI_GetParameterAsInt(2);
    jint y      = KNI_GetParameterAsInt(3);
    jint width  = KNI_GetParameterAsInt(4);
    jint height = KNI_GetParameterAsInt(5);

    if (handle == 0) {
        KNI_ReturnVoid();
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(pixels);
    KNI_GetParameterAsObject(6, pixels);
    if (!KNI_IsNullHandle(pixels)) {
        const void *src = SNI_GetRawArrayPointer(pixels);
        if (src != NULL) {
            m3gSetSubImage((M3GImage) handle, x, y, width, height,
                           KNI_GetArrayLength(pixels), src);
        }
    }
    KNI_EndHandles();

    KNI_ReturnVoid();
}

/* private static native void nCommit(int handle); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Image2D_nCommit()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gCommitImage((M3GImage) handle);
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * VertexArray
 *--------------------------------------------------------------------*/

/*
 * private static native int nCreate(int numVertices, int numComponents,
 *                                   int componentSize);
 *
 * JSR-184 names the element type by its size in bytes; m3gcore names it with
 * an M3Gdatatype (m3g/inc/M3G/m3g_core.h:309-316), and only accepts M3G_BYTE
 * and M3G_SHORT here (m3gcore/src/m3g_vertexarray.c:631).
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_VertexArray_nCreate()
{
    jint count = KNI_GetParameterAsInt(1);
    jint size  = KNI_GetParameterAsInt(2);
    jint bytes = KNI_GetParameterAsInt(3);

    M3Gdatatype type = (bytes == 1) ? M3G_BYTE : M3G_SHORT;

    KNI_ReturnInt(m3gOwn("VertexArray",
                         m3gCreateVertexArray(m3gPspGetInterface(),
                                              (M3Gsizei) count,
                                              size, type)));
}

/* private static native void nSetByte(int h, int first, int count, byte[] v); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_VertexArray_nSetByte()
{
    jint handle = KNI_GetParameterAsInt(1);
    jint first  = KNI_GetParameterAsInt(2);
    jint count  = KNI_GetParameterAsInt(3);

    if (handle == 0) {
        KNI_ReturnVoid();
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(values);
    KNI_GetParameterAsObject(4, values);
    if (!KNI_IsNullHandle(values)) {
        const void *src = SNI_GetRawArrayPointer(values);
        if (src != NULL) {
            m3gSetVertexArrayElements((M3GVertexArray) handle,
                                      first, (M3Gsizei) count,
                                      (M3Gsizei) KNI_GetArrayLength(values),
                                      M3G_BYTE, src);
        }
    }
    KNI_EndHandles();

    KNI_ReturnVoid();
}

/* private static native void nSetShort(int h, int first, int count, short[] v); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_VertexArray_nSetShort()
{
    jint handle = KNI_GetParameterAsInt(1);
    jint first  = KNI_GetParameterAsInt(2);
    jint count  = KNI_GetParameterAsInt(3);

    if (handle == 0) {
        KNI_ReturnVoid();
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(values);
    KNI_GetParameterAsObject(4, values);
    if (!KNI_IsNullHandle(values)) {
        const void *src = SNI_GetRawArrayPointer(values);
        if (src != NULL) {
            m3gSetVertexArrayElements((M3GVertexArray) handle,
                                      first, (M3Gsizei) count,
                                      (M3Gsizei) KNI_GetArrayLength(values),
                                      M3G_SHORT, src);
        }
    }
    KNI_EndHandles();

    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * VertexBuffer
 *--------------------------------------------------------------------*/

/* private static native int nCreate(); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_VertexBuffer_nCreate()
{
    KNI_ReturnInt(m3gOwn("VertexBuffer",
                         m3gCreateVertexBuffer(m3gPspGetInterface())));
}

/*
 * private static native void nSetPositions(int handle, int array,
 *                                          float scale, float[] bias);
 */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_VertexBuffer_nSetPositions()
{
    jint    handle = KNI_GetParameterAsInt(1);
    jint    array  = KNI_GetParameterAsInt(2);
    M3Gfloat scale = (M3Gfloat) KNI_GetParameterAsFloat(3);

    if (handle == 0) {
        KNI_ReturnVoid();
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(bias);
    KNI_GetParameterAsObject(4, bias);
    {
        M3Gfloat *b = NULL;
        M3Gint    n = 0;
        if (!KNI_IsNullHandle(bias)) {
            b = (M3Gfloat *) SNI_GetRawArrayPointer(bias);
            n = (M3Gint) KNI_GetArrayLength(bias);
        }
        m3gSetVertexArray((M3GVertexBuffer) handle,
                          (M3GVertexArray) array, scale, b, n);
    }
    KNI_EndHandles();

    KNI_ReturnVoid();
}

/* private static native void nSetNormals(int handle, int array); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_VertexBuffer_nSetNormals()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetNormalArray((M3GVertexBuffer) handle,
                          (M3GVertexArray) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetColors(int handle, int array); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_VertexBuffer_nSetColors()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetColorArray((M3GVertexBuffer) handle,
                         (M3GVertexArray) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/*
 * private static native void nSetTexCoords(int handle, int unit, int array,
 *                                          float scale, float[] bias);
 */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_VertexBuffer_nSetTexCoords()
{
    jint     handle = KNI_GetParameterAsInt(1);
    jint     unit   = KNI_GetParameterAsInt(2);
    jint     array  = KNI_GetParameterAsInt(3);
    M3Gfloat scale  = (M3Gfloat) KNI_GetParameterAsFloat(4);

    if (handle == 0) {
        KNI_ReturnVoid();
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(bias);
    KNI_GetParameterAsObject(5, bias);
    {
        M3Gfloat *b = NULL;
        M3Gint    n = 0;
        if (!KNI_IsNullHandle(bias)) {
            b = (M3Gfloat *) SNI_GetRawArrayPointer(bias);
            n = (M3Gint) KNI_GetArrayLength(bias);
        }
        m3gSetTexCoordArray((M3GVertexBuffer) handle, unit,
                            (M3GVertexArray) array, scale, b, n);
    }
    KNI_EndHandles();

    KNI_ReturnVoid();
}

/* private static native void nSetDefaultColor(int handle, int argb); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_VertexBuffer_nSetDefaultColor()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetVertexDefaultColor((M3GVertexBuffer) handle,
                                 (M3Guint) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * TriangleStripArray
 *
 * Java int[] and the engine's M3Gsizei[] / M3G_INT index arrays have the same
 * layout, so both go straight through without a copy.
 *--------------------------------------------------------------------*/

/* private static native int nCreateImplicit(int firstIndex, int[] lengths); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_TriangleStripArray_nCreateImplicit()
{
    jint firstIndex = KNI_GetParameterAsInt(1);
    jint result = 0;

    KNI_StartHandles(1);
    KNI_DeclareHandle(lengths);
    KNI_GetParameterAsObject(2, lengths);
    if (!KNI_IsNullHandle(lengths)) {
        const M3Gsizei *sl = (const M3Gsizei *) SNI_GetRawArrayPointer(lengths);
        if (sl != NULL) {
            result = m3gOwn("TriangleStripArray",
                            m3gCreateImplicitStripBuffer(
                                m3gPspGetInterface(),
                                (M3Gsizei) KNI_GetArrayLength(lengths),
                                sl, firstIndex));
        }
    }
    KNI_EndHandles();

    KNI_ReturnInt(result);
}

/* private static native int nCreateExplicit(int[] indices, int[] lengths); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_TriangleStripArray_nCreateExplicit()
{
    jint result = 0;

    KNI_StartHandles(2);
    KNI_DeclareHandle(indices);
    KNI_DeclareHandle(lengths);
    KNI_GetParameterAsObject(1, indices);
    KNI_GetParameterAsObject(2, lengths);
    if (!KNI_IsNullHandle(indices) && !KNI_IsNullHandle(lengths)) {
        const void     *idx = SNI_GetRawArrayPointer(indices);
        const M3Gsizei *sl  = (const M3Gsizei *) SNI_GetRawArrayPointer(lengths);
        if (idx != NULL && sl != NULL) {
            result = m3gOwn("TriangleStripArray",
                            m3gCreateStripBuffer(
                                m3gPspGetInterface(),
                                M3G_TRIANGLE_STRIPS,
                                (M3Gsizei) KNI_GetArrayLength(lengths),
                                sl,
                                M3G_INT,
                                (M3Gsizei) KNI_GetArrayLength(indices),
                                idx));
        }
    }
    KNI_EndHandles();

    KNI_ReturnInt(result);
}

/*----------------------------------------------------------------------
 * Mesh and its two subclasses
 *
 * The submesh and appearance arrays arrive as Java int[] of handles, which is
 * bit-for-bit the M3GIndexBuffer[] / M3GAppearance[] the engine wants.  A null
 * appearance array is passed through as NULL, which m3gInitMesh reads as "no
 * appearances" (m3gcore/src/m3g_mesh.c:463); individual null entries are fine
 * (:460, M3G_ASSIGN_REF tolerates NULL).
 *--------------------------------------------------------------------*/

/*
 * private static native int nCreate(int vertices, int[] submeshes,
 *                                   int[] appearances);
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Mesh_nCreate()
{
    jint vertices = KNI_GetParameterAsInt(1);
    jint result = 0;

    KNI_StartHandles(2);
    KNI_DeclareHandle(submeshes);
    KNI_DeclareHandle(appearances);
    KNI_GetParameterAsObject(2, submeshes);
    KNI_GetParameterAsObject(3, appearances);
    if (vertices != 0 && !KNI_IsNullHandle(submeshes)) {
        M3GIndexBuffer *ib =
            (M3GIndexBuffer *) SNI_GetRawArrayPointer(submeshes);
        M3GAppearance *ap = KNI_IsNullHandle(appearances)
            ? NULL : (M3GAppearance *) SNI_GetRawArrayPointer(appearances);
        if (ib != NULL) {
            result = m3gOwn("Mesh",
                            m3gCreateMesh(m3gPspGetInterface(),
                                          (M3GVertexBuffer) vertices,
                                          ib, ap,
                                          (M3Gint) KNI_GetArrayLength(submeshes)));
        }
    }
    KNI_EndHandles();

    KNI_ReturnInt(result);
}

/* private static native void nSetAppearance(int h, int index, int appearance); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Mesh_nSetAppearance()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetAppearance((M3GMesh) handle,
                         KNI_GetParameterAsInt(2),
                         (M3GAppearance) KNI_GetParameterAsInt(3));
    }
    KNI_ReturnVoid();
}

/*
 * private static native int nCreate(int vertices, int[] submeshes,
 *                                   int[] appearances, int skeleton);
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_SkinnedMesh_nCreate()
{
    jint vertices = KNI_GetParameterAsInt(1);
    jint skeleton = KNI_GetParameterAsInt(4);
    jint result = 0;

    KNI_StartHandles(2);
    KNI_DeclareHandle(submeshes);
    KNI_DeclareHandle(appearances);
    KNI_GetParameterAsObject(2, submeshes);
    KNI_GetParameterAsObject(3, appearances);
    if (vertices != 0 && skeleton != 0 && !KNI_IsNullHandle(submeshes)) {
        M3GIndexBuffer *ib =
            (M3GIndexBuffer *) SNI_GetRawArrayPointer(submeshes);
        M3GAppearance *ap = KNI_IsNullHandle(appearances)
            ? NULL : (M3GAppearance *) SNI_GetRawArrayPointer(appearances);
        if (ib != NULL) {
            result = m3gOwn("SkinnedMesh",
                            m3gCreateSkinnedMesh(
                                m3gPspGetInterface(),
                                (M3GVertexBuffer) vertices,
                                ib, ap,
                                (M3Gint) KNI_GetArrayLength(submeshes),
                                (M3GGroup) skeleton));
        }
    }
    KNI_EndHandles();

    KNI_ReturnInt(result);
}

/*
 * private static native void nAddTransform(int handle, int bone, int weight,
 *                                          int firstVertex, int numVertices);
 */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_SkinnedMesh_nAddTransform()
{
    jint handle = KNI_GetParameterAsInt(1);
    jint bone   = KNI_GetParameterAsInt(2);

    if (handle != 0 && bone != 0) {
        m3gAddTransform((M3GSkinnedMesh) handle,
                        (M3GNode) bone,
                        KNI_GetParameterAsInt(3),
                        KNI_GetParameterAsInt(4),
                        KNI_GetParameterAsInt(5));
    }
    KNI_ReturnVoid();
}

/*
 * private static native int nCreate(int vertices, int[] targets,
 *                                   int[] submeshes, int[] appearances);
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_MorphingMesh_nCreate()
{
    jint vertices = KNI_GetParameterAsInt(1);
    jint result = 0;

    KNI_StartHandles(3);
    KNI_DeclareHandle(targets);
    KNI_DeclareHandle(submeshes);
    KNI_DeclareHandle(appearances);
    KNI_GetParameterAsObject(2, targets);
    KNI_GetParameterAsObject(3, submeshes);
    KNI_GetParameterAsObject(4, appearances);
    if (vertices != 0
        && !KNI_IsNullHandle(targets) && !KNI_IsNullHandle(submeshes)) {
        M3GVertexBuffer *tg =
            (M3GVertexBuffer *) SNI_GetRawArrayPointer(targets);
        M3GIndexBuffer *ib =
            (M3GIndexBuffer *) SNI_GetRawArrayPointer(submeshes);
        M3GAppearance *ap = KNI_IsNullHandle(appearances)
            ? NULL : (M3GAppearance *) SNI_GetRawArrayPointer(appearances);
        if (tg != NULL && ib != NULL) {
            result = m3gOwn("MorphingMesh",
                            m3gCreateMorphingMesh(
                                m3gPspGetInterface(),
                                (M3GVertexBuffer) vertices,
                                tg, ib, ap,
                                (M3Gint) KNI_GetArrayLength(submeshes),
                                (M3Gint) KNI_GetArrayLength(targets)));
        }
    }
    KNI_EndHandles();

    KNI_ReturnInt(result);
}

/* private static native void nSetWeights(int handle, float[] weights); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_MorphingMesh_nSetWeights()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle == 0) {
        KNI_ReturnVoid();
    }

    KNI_StartHandles(1);
    KNI_DeclareHandle(weights);
    KNI_GetParameterAsObject(2, weights);
    if (!KNI_IsNullHandle(weights)) {
        M3Gfloat *w = (M3Gfloat *) SNI_GetRawArrayPointer(weights);
        if (w != NULL) {
            m3gSetWeights((M3GMorphingMesh) handle, w,
                          (M3Gint) KNI_GetArrayLength(weights));
        }
    }
    KNI_EndHandles();

    KNI_ReturnVoid();
}

/*----------------------------------------------------------------------
 * Sprite3D
 *--------------------------------------------------------------------*/

/* private static native int nCreate(int scaled, int image, int appearance); */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Sprite3D_nCreate()
{
    jint scaled     = KNI_GetParameterAsInt(1);
    jint image      = KNI_GetParameterAsInt(2);
    jint appearance = KNI_GetParameterAsInt(3);

    if (image == 0) {
        KNI_ReturnInt(0);
    }
    KNI_ReturnInt(m3gOwn("Sprite3D",
                         m3gCreateSprite(m3gPspGetInterface(),
                                         (M3Gbool) (scaled ? M3G_TRUE
                                                           : M3G_FALSE),
                                         (M3GImage) image,
                                         (M3GAppearance) appearance)));
}

/* private static native void nSetImage(int handle, int image); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Sprite3D_nSetImage()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetSpriteImage((M3GSprite) handle,
                          (M3GImage) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetAppearance(int handle, int appearance); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Sprite3D_nSetAppearance()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetSpriteAppearance((M3GSprite) handle,
                               (M3GAppearance) KNI_GetParameterAsInt(2));
    }
    KNI_ReturnVoid();
}

/* private static native void nSetCrop(int h, int x, int y, int w, int height); */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Sprite3D_nSetCrop()
{
    jint handle = KNI_GetParameterAsInt(1);

    if (handle != 0) {
        m3gSetCrop((M3GSprite) handle,
                   KNI_GetParameterAsInt(2),
                   KNI_GetParameterAsInt(3),
                   KNI_GetParameterAsInt(4),
                   KNI_GetParameterAsInt(5));
    }
    KNI_ReturnVoid();
}
