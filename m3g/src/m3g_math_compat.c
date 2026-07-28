/*
 * m3g_math_compat.c -- the older-generation matrix entry points that the
 * m3gcore sources still call.
 *
 * Five names are referenced from 18 call sites in the m3gcore drop but are
 * defined nowhere in it:
 *
 *     m3gMulMatrix          m3gRightMulMatrix     m3gTranslateMatrix
 *     m3gScaleMatrix        m3gRotateMatrixQuat
 *
 * src/m3g_math.c only ships the later Pre-/Post- spelling of the same
 * operations (m3gPreMultiplyMatrix at src/m3g_math.c:2636,
 * m3gPostMultiplyMatrix at :2623, and the Pre/Post Rotate/Scale/Translate
 * wrappers at :2649-:2731).  Each function below is therefore a thin
 * forwarder onto one of those.
 *
 *
 * 1. Matrix convention
 * --------------------
 * The library is column-major with column vectors, i.e. v' = M * v:
 *
 *   - src/m3g_math.c:72-74  "The current convention is column-major, as in
 *     OpenGL ES", MELEM(row,col) == row + col*4.
 *   - src/m3g_math.c:3002 m3gTransformVec4 computes
 *     vec[i] = M44F(mtx,i,0)*v.x + M44F(mtx,i,1)*v.y + ... -- a row of the
 *     matrix dotted with the vector, i.e. M*v.
 *   - src/m3g_math.c:3044 m3gTranslationMatrix stores tx,ty,tz in
 *     M44F(mtx,0..2,3), the last *column*.
 *   - src/m3g_camera.c:77 sets m[11] = -1 for the perspective matrix and
 *     feeds it through m3gSetMatrixColumns (:107), the standard GL
 *     column-vector frustum.
 *
 * Consequently "post-multiply" (mtx = mtx * other) means `other` is applied
 * to the vertex *first*, and "pre-multiply" (mtx = other * mtx) means
 * `other` is applied *last*.  Every one of the five functions below turns
 * out to be a post-multiply; the per-function evidence is given at each.
 *
 *
 * 2. Argument types: the float parameters really are `double`
 * -----------------------------------------------------------
 * None of these five names is declared in any m3gcore header (verified by
 * grepping inc/ and src/; gcc reports "implicit declaration of function" for
 * all five, e.g. at src/m3g_skinnedmesh.c:888 and src/m3g_transformable.c:257).
 * The calls are therefore made through an implicit, unprototyped declaration,
 * which applies the default argument promotions -- so the M3Gfloat (float)
 * arguments arrive as **double**.  Defining these with `M3Gfloat` parameters
 * would compile and link but read the wrong bytes at run time and silently
 * corrupt every translation and scale.  The parameters below are `double`
 * for exactly that reason, and are narrowed back to M3Gfloat on the way in.
 *
 * If prototypes for these functions are ever added to the m3gcore headers,
 * the parameter types here must change to M3Gfloat at the same time.
 *
 * The pointer parameters need no such care.  `const M3GMatrix *` is used
 * where the value is only read; src/m3g_rendercontext.c:1842 passes a
 * genuinely const matrix, the other sites pass non-const ones.
 */

#include "M3G/m3g_core.h"

/*!
 * m3gMulMatrix(mtx, other) -- mtx = mtx * other.
 *
 * Delegates to m3gPostMultiplyMatrix (src/m3g_math.c:2623).
 *
 * Evidence for post- rather than pre-multiplication:
 *
 *  - src/m3g_transformable.c:567-575 builds the JSR-184 composite transform
 *    from the identity in the order translate, rotate, scale, generic
 *    matrix.  The spec defines that composite as T * R * S * M applied as
 *    v' = T*R*S*M*v, which with column vectors is exactly what repeated
 *    post-multiplication produces.  Pre-multiplication would yield M*S*R*T.
 *  - src/m3g_node.c:642-647, m3gGetTransformUpPath: `transform` already
 *    holds parent->ancestor and `mtx` holds node->parent (the node composite
 *    transform); the documented result (:616 "transformation to an ancestor
 *    node") is node->ancestor = (parent->ancestor) * (node->parent), i.e.
 *    transform * mtx.
 *  - src/m3g_group.c:255-258 and src/m3g_camera.c:274-277, walking *up* the
 *    tree during render setup: s->toCamera holds self->camera, `t` holds
 *    parent->self (m3gGetInverseNodeTransform), and the parent needs
 *    parent->camera = (self->camera) * (parent->self) = toCamera * t.
 *    The same function's downward branch, src/m3g_group.c:223-224, does the
 *    mirror image with the surviving API: composite(child) followed by
 *    m3gPreMultiplyMatrix(&cs.toCamera, &s->toCamera) = toCamera * composite.
 *  - src/m3g_rendercontext.c:1840-1843: view matrix, then the caller's model
 *    transform, giving modelview = V * M.
 *  - src/m3g_skinnedmesh.c:869-872: bone->mesh from m3gGetTransformTo, then
 *    the at-rest mesh->bone matrix, giving (bone->mesh) * (mesh->bone).
 */
void m3gMulMatrix(M3GMatrix *mtx, const M3GMatrix *other)
{
    m3gPostMultiplyMatrix(mtx, other);
}

/*!
 * m3gRightMulMatrix(mtx, other) -- mtx = mtx * other.
 *
 * Delegates to m3gPostMultiplyMatrix (src/m3g_math.c:2623).
 *
 * The name suggests the opposite hand of m3gMulMatrix, but both of its call
 * sites in this drop unambiguously require the destination to stay on the
 * *left* of the product, i.e. the same operation as m3gMulMatrix:
 *
 *  - src/m3g_node.c:1109-1143, m3gGetTransformTo (":1032 Gets transform from
 *    node to another", so the result maps node space to target space).
 *    `targetPath` has been inverted at :1123 and therefore holds
 *    pivot->target; `sourcePath` from :1139 holds node->pivot.  The result
 *    must be node->target = (pivot->target) * (node->pivot), i.e.
 *    targetPath * sourcePath -- destination on the left.
 *    Worked example: A rotated 90 deg about Z under the root, B translated
 *    x+20 under the root.  A->B must map (1,0,0) to (-20,1,0), which is
 *    T(-20)*Rz(90); the other order, Rz(90)*T(-20), gives (0,-19,0).
 *  - src/m3g_group.c:331-336, m3gGroupRayIntersect.  `toGroup` is the
 *    node->pick-root transform: it starts as the identity at the pick root
 *    (src/m3g_group.c:1005-1007) and src/m3g_mesh.c:218-226 *inverts* it to
 *    take the ray from pick-root space into mesh space.  Descending one
 *    level, child->pickRoot = (group->pickRoot) * (child->group) = t * nt --
 *    again destination on the left.
 *
 * So in this source drop m3gRightMulMatrix and m3gMulMatrix are the same
 * operation.  Implementing it as a pre-multiply would break both picking and
 * Node.getTransformTo.
 */
void m3gRightMulMatrix(M3GMatrix *mtx, const M3GMatrix *other)
{
    m3gPostMultiplyMatrix(mtx, other);
}

/*!
 * m3gTranslateMatrix(mtx, tx, ty, tz) -- mtx = mtx * T(tx,ty,tz).
 *
 * Delegates to m3gPostTranslateMatrix (src/m3g_math.c:2681).
 *
 * Evidence:
 *
 *  - src/m3g_transformable.c:567-570: identity, then this call, then the
 *    rotation and scale, must produce the spec's T * R * S composite (see
 *    m3gMulMatrix above).  Only post-multiplication puts T on the outside
 *    when it is applied first.
 *  - src/m3g_transformable.c:253-259 builds the *inverse* composite: the
 *    matrix already holds (S*M)^-1 = M^-1 * S^-1 after the inversion at
 *    :245, then gets the conjugate rotation and finally this call with the
 *    negated translation.  Post-multiplying yields
 *    M^-1 * S^-1 * R^-1 * T^-1 = (T*R*S*M)^-1, the correct inverse of the
 *    composite built at :567-575.  Pre-multiplying would give
 *    T^-1 * R^-1 * M^-1 * S^-1, which is not the inverse of anything the
 *    library builds.
 *  - src/m3g_skinnedmesh.c:886-890: the comment at :886 is "Apply the vertex
 *    bias and scale to the transformation"; the fixed-point vertices are
 *    decoded as scale*v + bias, i.e. T(bias) * S(scale) applied before the
 *    bone transform already in `mtx`.
 *
 * `tx`, `ty`, `tz` are `double` -- see the note at the top of this file.
 */
void m3gTranslateMatrix(M3GMatrix *mtx, double tx, double ty, double tz)
{
    m3gPostTranslateMatrix(mtx, (M3Gfloat) tx, (M3Gfloat) ty, (M3Gfloat) tz);
}

/*!
 * m3gScaleMatrix(mtx, sx, sy, sz) -- mtx = mtx * S(sx,sy,sz).
 *
 * Delegates to m3gPostScaleMatrix (src/m3g_math.c:2671).
 *
 * Evidence, strongest first:
 *
 *  - src/m3g_skinnedmesh.c:984-996 is decisive on its own.  A translation
 *    matrix is built at :986, this function is applied to it at :990, and
 *    the result is then split into a fixed-point 3x3 basis (:994) and a
 *    fixed-point translation (:996) that the vertex pipeline applies as
 *    scale*v + bias.  T * S has 3x3 = S and translation = bias, which is
 *    what that split needs; S * T would leave the *scaled* bias in the
 *    translation column and decode every unskinned vertex wrongly.
 *  - src/m3g_transformable.c:567-570: the S of the spec's T * R * S * M
 *    composite, applied third, must end up third from the left.
 *  - src/m3g_skinnedmesh.c:888-890, as for m3gTranslateMatrix above.
 *
 * `sx`, `sy`, `sz` are `double` -- see the note at the top of this file.
 */
void m3gScaleMatrix(M3GMatrix *mtx, double sx, double sy, double sz)
{
    m3gPostScaleMatrix(mtx, (M3Gfloat) sx, (M3Gfloat) sy, (M3Gfloat) sz);
}

/*!
 * m3gRotateMatrixQuat(mtx, quat) -- mtx = mtx * R(quat).
 *
 * Delegates to m3gPostRotateMatrixQuat (src/m3g_math.c:2661).
 *
 * Evidence:
 *
 *  - src/m3g_transformable.c:567-570: the R of the spec's T * R * S * M
 *    composite, applied second, must end up second from the left -- which
 *    post-multiplication after the translate produces.
 *  - src/m3g_transformable.c:254-259, the inverse composite: the conjugate
 *    quaternion (:255-256, w negated) is applied to M^-1 * S^-1 and must
 *    give M^-1 * S^-1 * R^-1 before the final inverse translation.  See
 *    m3gTranslateMatrix above for the full derivation.
 *  - Symmetry with src/m3g_node.c:493-499, which builds the same kind of
 *    inverse (negated translation, conjugated quaternion) with the surviving
 *    API and uses m3gPreTranslateMatrix / m3gPreRotateMatrixQuat there
 *    because that matrix is composed in the opposite direction -- it starts
 *    from the target->parent path and prepends the parent->node components.
 */
void m3gRotateMatrixQuat(M3GMatrix *mtx, const M3GQuat *quat)
{
    m3gPostRotateMatrixQuat(mtx, quat);
}
