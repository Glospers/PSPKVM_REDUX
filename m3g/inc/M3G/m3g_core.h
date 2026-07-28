/*
 * Public interface of the M3G (JSR-184) core library.
 *
 * This header was absent from the m3gcore source drop. It has been
 * reconstructed from the way the rest of the tree uses it: every type,
 * constant and prototype below is referenced from inc/ or src/. Nothing
 * that is not referenced has been added.
 *
 * Included by:
 *   inc/m3g_defs.h:28, inc/m3g_gl.h:27, inc/m3g_memory.h:31
 *
 * Ordering / numbering of the public enumerations that the .m3g file loader
 * feeds straight from file data (src/m3g_loader.c reads a byte or an int and
 * passes it to the setter unchanged) follows the JSR-184 binary file format,
 * i.e. the values of the corresponding javax.microedition.m3g constants.
 * The evidence for each is cited inline.
 */

#ifndef __M3G_CORE_H__
#define __M3G_CORE_H__

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
 * Calling convention / linkage
 *
 * Used on every public entry point, e.g. src/m3g_fog.c:273.
 * m3g_symbian.cpp:214 tests M3G_BUILD_DLL, so a DLL build is the only
 * case that needs anything other than plain external linkage.
 *
 * NOTE: kept as plain `extern` for static builds because five public
 * functions are *defined* without the macro (src/m3g_polygonmode.c:232,
 * :245, src/m3g_sprite.c:996, src/m3g_rendercontext.inl:1440, :1523);
 * a __declspec()-style expansion would clash with those definitions.
 *--------------------------------------------------------------------*/

#if !defined(M3G_API)
#   if defined(M3G_BUILD_DLL) && (defined(_WIN32) || defined(_WIN64))
#       define M3G_API __declspec(dllexport)
#   else
#       define M3G_API extern
#   endif
#endif

/*----------------------------------------------------------------------
 * Scalar types
 *
 * Sizes are asserted at compile time in inc/m3g_defs.h:599-609.
 *--------------------------------------------------------------------*/

typedef signed char     M3Gbyte;    /* sizeof == 1, m3g_defs.h:599 */
typedef unsigned char   M3Gubyte;   /* sizeof == 1, m3g_defs.h:600 */
typedef short           M3Gshort;   /* sizeof == 2, m3g_defs.h:601 */
typedef unsigned short  M3Gushort;  /* sizeof == 2, m3g_defs.h:602 */
typedef int             M3Gint;     /* sizeof == 4, m3g_defs.h:603 */
typedef unsigned int    M3Guint;    /* sizeof == 4, m3g_defs.h:604 */
typedef float           M3Gfloat;   /* sizeof == 4, m3g_defs.h:605 */

/*!
 * \brief Boolean
 *
 * NOTE: inferred unsigned from inc/m3g_image.h:61-66, where M3Gbool is used
 * as a bit-field base type and `M3Gbool mipDataMapCount : 4` is documented
 * as holding "max. 16 concurrent uses" -- only reachable if unsigned.
 * M3Gint compiles equally well; only the comment discriminates.
 */
typedef M3Guint         M3Gbool;

/*!
 * \brief Bit mask
 *
 * NOTE: inferred unsigned from inc/m3g_image.h:58-59, where M3Gbitmask is a
 * bit-field base type. M3Gint compiles equally well.
 */
typedef M3Guint         M3Gbitmask;

/*!
 * \brief Enumerated value; src/m3g_interface.c:72, src/m3g_node.c:46
 *
 * This one is pinned, not guessed: inc/m3g_transformable.h:74-75 declares
 * m3gTransformableIsCompatible/m3gTransformableUpdateProperty taking
 * M3Gint, while src/m3g_transformable.c:107,128 define the same two
 * functions taking M3Genum. The tree only compiles if the two types are
 * identical, so M3Genum must be M3Gint (signed), not an unsigned GLenum
 * analogue.
 */
typedef M3Gint          M3Genum;

/*!
 * \brief Signed size/count, as in GLsizei; src/m3g_indexbuffer.c:301
 *
 * NOTE: signedness inferred from the GL naming convention; nothing in the
 * tree distinguishes M3Gint from M3Guint here.
 */
typedef M3Gint          M3Gsizei;

/* Truth values.
 *
 * Plain macros rather than enumerators: they are used as the controlling
 * expression of do/while(M3G_FALSE) in inc/m3g_defs.h:527 and as the value
 * of configuration macros such as M3G_SUPPORT_MIPMAPPING (m3g_defs.h:301).
 */
#define M3G_FALSE       0
#define M3G_TRUE        1

/*----------------------------------------------------------------------
 * Opaque object handles
 *
 * Each is a pointer to the matching implementation struct forward-declared
 * in src/m3g_core.c:34-60 (and src/m3g_loader.c:95 for the Loader). The
 * implementations cast straight between them and the internal type, e.g.
 * `Interface *m3g = (Interface *) hInterface` (src/m3g_interface.c:1856),
 * and return NULL on failure (src/m3g_interface.c:1600).
 *--------------------------------------------------------------------*/

typedef struct M3GInterfaceImpl             *M3GInterface;
typedef struct M3GObjectImpl                *M3GObject;
typedef struct M3GTransformableImpl         *M3GTransformable;
typedef struct M3GNodeImpl                  *M3GNode;
typedef struct M3GGroupImpl                 *M3GGroup;
typedef struct M3GWorldImpl                 *M3GWorld;
typedef struct M3GCameraImpl                *M3GCamera;
typedef struct M3GLightImpl                 *M3GLight;
typedef struct M3GBackgroundImpl            *M3GBackground;
typedef struct M3GFogImpl                   *M3GFog;
typedef struct M3GMaterialImpl              *M3GMaterial;
typedef struct M3GCompositingModeImpl       *M3GCompositingMode;
typedef struct M3GPolygonModeImpl           *M3GPolygonMode;
typedef struct M3GTextureImpl               *M3GTexture;
typedef struct M3GAppearanceImpl            *M3GAppearance;
typedef struct M3GMeshImpl                  *M3GMesh;
typedef struct M3GMorphingMeshImpl          *M3GMorphingMesh;
typedef struct M3GSkinnedMeshImpl           *M3GSkinnedMesh;
typedef struct M3GSpriteImpl                *M3GSprite;
typedef struct M3GVertexArrayImpl           *M3GVertexArray;
typedef struct M3GVertexBufferImpl          *M3GVertexBuffer;
typedef struct M3GIndexBufferImpl           *M3GIndexBuffer;
typedef struct M3GImageImpl                 *M3GImage;
typedef struct M3GRenderContextImpl         *M3GRenderContext;
typedef struct M3GAnimationControllerImpl   *M3GAnimationController;
typedef struct M3GAnimationTrackImpl        *M3GAnimationTrack;
typedef struct M3GKeyframeSequenceImpl      *M3GKeyframeSequence;
typedef struct M3GLoaderImpl                *M3GLoader;

/*----------------------------------------------------------------------
 * Integer handles
 *
 * These are integers, not pointers: they are assigned to and from the
 * M3Guint field RenderContext::target.handle without a cast
 * (src/m3g_rendercontext.inl:753, src/m3g_rendercontext.c:516), which is
 * safe because inc/m3g_defs.h:609 asserts sizeof(M3Guint) >= sizeof(void*).
 *--------------------------------------------------------------------*/

/*! \brief Handle to a block of memory owned by the application */
typedef M3Guint M3GMemObject;       /* src/m3g_interface.c:230, inc/m3g_image.h:54 */

/*!
 * \brief Native platform bitmap handle
 *
 * inc/m3g_gl.h:89; initialised straight from the M3Guint target.handle at
 * src/m3g_rendercontext.inl:753 and passed to the M3Guint parameter of
 * m3gBindRenderTarget at :1459, both without a cast.
 *
 * NOTE: src/m3g_rendercontext.inl:275 and :356 pass a literal NULL for this
 * argument of m3gQueryEGLConfig, which reads as a pointer type instead. The
 * integer reading is used because it is the one the majority of the call
 * sites and the design note at inc/m3g_defs.h:607-609 ("Unsigned is used
 * extensively as a wrapper for object pointers", plus the assertion that
 * sizeof(M3Guint) >= sizeof(void*)) require; a pointer typedef produces
 * strictly more type mismatches. Those two NULLs mean "no bitmap" and
 * should be 0.
 */
typedef M3Guint M3GNativeBitmap;

/*! \brief Native platform window handle; inc/m3g_gl.h:97, src/m3g_rendercontext.inl:1564 */
typedef M3Guint M3GNativeWindow;

/*! \brief An EGLSurface cast to an integer; src/m3g_rendercontext.inl:1476,1485 */
typedef M3Guint M3GEGLSurface;

/*!
 * \brief An EGLContext cast to an integer
 *
 * NOTE: inferred. The only reference in the drop is the Splint annotation
 * `/ *@access M3GGLContext@* /` at src/m3g_rendercontext.inl:1522; no C code
 * uses it. Declared for symmetry with M3GEGLSurface.
 */
typedef M3Guint M3GGLContext;

/*----------------------------------------------------------------------
 * Public math types
 *
 * Layout of M3GVec4 and M3GQuat is asserted to be four floats in
 * inc/m3g_defs.h:853-854; both are brace-initialised from four scalars in
 * inc/m3g_math.h:110-113. Aliased to Matrix/Quat/Vec3/Vec4/Rect in
 * inc/m3g_defs.h:844-848.
 *--------------------------------------------------------------------*/

/*! \brief 3-vector; members from src/m3g_math.c:1409-1422 (tuv->x/y/z) */
typedef struct {
    M3Gfloat x, y, z;
} M3GVec3;

/*! \brief 4-vector */
typedef struct {
    M3Gfloat x, y, z, w;
} M3GVec4;

/*! \brief Quaternion; members from src/m3g_math.c:1281-1303 */
typedef struct {
    M3Gfloat x, y, z, w;
} M3GQuat;

/*!
 * \brief 4x4 matrix, column-major as in OpenGL ES
 *
 * Members are read directly by src/m3g_math.c: `elem` is a 16-float array
 * (m3g_math.c:84, :2221 sizeof(mtx->elem)), `mask` is a 32-bit
 * classification mask (m3g_math.c:871, compared against 0xFFFFFFFF at :53),
 * and `complete` / `classified` are flags (m3g_math.c:828, :884).
 *
 * NOTE: `complete` and `classified` are cast to M3Gbool on read
 * (src/m3g_math.c:828), so the original may have declared them as
 * single-bit fields; plain M3Gbool is used here.
 */
typedef struct {
    M3Gfloat elem[16];
    M3Guint  mask;
    M3Gbool  complete;
    M3Gbool  classified;
} M3GMatrix;

/*! \brief Integer rectangle; members from src/m3g_math.c:1537-1547 */
typedef struct {
    M3Gint x, y, width, height;
} M3GRectangle;

/*----------------------------------------------------------------------
 * Error codes
 *
 * Returned by m3gGetError and passed to m3gRaiseError / the error callback
 * (src/m3g_interface.c:1198-1201). M3G_NO_ERROR must be the "cleared" value
 * (src/m3g_interface.c:1201).
 *--------------------------------------------------------------------*/

typedef enum {
    M3G_NO_ERROR            = 0,    /* src/m3g_interface.c:1201 */
    M3G_INVALID_VALUE,              /* src/m3g_fog.c:307 */
    M3G_INVALID_ENUM,               /* src/m3g_interface.c:1869 */
    M3G_INVALID_OPERATION,          /* src/m3g_rendercontext.c:491 */
    M3G_INVALID_OBJECT,             /* src/m3g_object.c:456 */
    M3G_INVALID_INDEX,              /* src/m3g_appearance.c:501 */
    M3G_NULL_POINTER,               /* src/m3g_animationtrack.c:305 */
    M3G_OUT_OF_MEMORY,              /* src/m3g_interface.c:530 */
    M3G_ARITHMETIC_ERROR,           /* src/m3g_camera.c:559 */
    M3G_IO_ERROR                    /* src/m3g_loader.c:504 */
} M3GError;

/*----------------------------------------------------------------------
 * Class identifiers
 *
 * The order below is fixed by the virtual function table in
 * src/m3g_core.c:179-209, whose comment states that it "must correspond to
 * the class ID enumeration defined in m3g_core.h". M3G_ABSTRACT_CLASS is
 * index 0 and M3G_CLASS_WORLD is the last entry; src/m3g_object.c:49 also
 * asserts the concrete IDs form the contiguous range
 * [M3G_CLASS_ANIMATION_CONTROLLER, M3G_CLASS_WORLD].
 *--------------------------------------------------------------------*/

typedef enum {
    M3G_ABSTRACT_CLASS              = 0,
    M3G_CLASS_ANIMATION_CONTROLLER,
    M3G_CLASS_ANIMATION_TRACK,
    M3G_CLASS_APPEARANCE,
    M3G_CLASS_BACKGROUND,
    M3G_CLASS_CAMERA,
    M3G_CLASS_COMPOSITING_MODE,
    M3G_CLASS_FOG,
    M3G_CLASS_GROUP,
    M3G_CLASS_IMAGE,
    M3G_CLASS_INDEX_BUFFER,
    M3G_CLASS_KEYFRAME_SEQUENCE,
    M3G_CLASS_LIGHT,
    M3G_CLASS_LOADER,
    M3G_CLASS_MATERIAL,
    M3G_CLASS_MESH,
    M3G_CLASS_MORPHING_MESH,
    M3G_CLASS_POLYGON_MODE,
    M3G_CLASS_RENDER_CONTEXT,
    M3G_CLASS_SKINNED_MESH,
    M3G_CLASS_SPRITE,
    M3G_CLASS_TEXTURE,
    M3G_CLASS_VERTEX_ARRAY,
    M3G_CLASS_VERTEX_BUFFER,
    M3G_CLASS_WORLD
} M3GClass;

/*----------------------------------------------------------------------
 * Vertex array element types
 *
 * The values are pinned by inc/m3g_gl.h:68, M3G_GLTYPE(t) == t + 0x1400:
 * adding 0x1400 must yield GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT,
 * GL_UNSIGNED_SHORT, GL_INT and GL_UNSIGNED_INT respectively.
 * Used at src/m3g_vertexarray.c:480-501, :668, :706.
 *--------------------------------------------------------------------*/

typedef enum {
    M3G_BYTE    = 0,    /* GL_BYTE           0x1400 */
    M3G_UBYTE   = 1,    /* GL_UNSIGNED_BYTE  0x1401 */
    M3G_SHORT   = 2,    /* GL_SHORT          0x1402 */
    M3G_USHORT  = 3,    /* GL_UNSIGNED_SHORT 0x1403 */
    M3G_INT     = 4,    /* GL_INT            0x1404 */
    M3G_UINT    = 5     /* GL_UNSIGNED_INT   0x1405 */
} M3Gdatatype;

/*----------------------------------------------------------------------
 * Index buffer primitive types
 *
 * Only one value is ever referenced (src/m3g_indexbuffer.c:269, :308, :553);
 * the comment at m3g_indexbuffer.c:284 says "always M3G_TRIANGLE_STRIPS".
 *--------------------------------------------------------------------*/

typedef enum {
    M3G_TRIANGLE_STRIPS = 0
} M3Gprimitive;

/*----------------------------------------------------------------------
 * Logical (user-visible) image formats
 *
 * Values are the javax.microedition.m3g.Image2D format constants: the
 * loader takes the format straight out of the file as an unsigned byte
 * (src/m3g_loader.c:2129 `format = (M3GImageFormat)*data++`). The range
 * check at src/m3g_image.c:1234, m3gInRange(srcFormat, M3G_ALPHA, M3G_RGBA),
 * requires them to be contiguous with M3G_ALPHA lowest and M3G_RGBA highest;
 * the intermediate order follows src/m3g_image.c:767-776 and
 * src/m3g_loader.c:2145-2149.
 *--------------------------------------------------------------------*/

typedef enum {
    M3G_ALPHA               = 96,
    M3G_LUMINANCE           = 97,
    M3G_LUMINANCE_ALPHA     = 98,
    M3G_RGB                 = 99,
    M3G_RGBA                = 100
} M3GImageFormat;

/*----------------------------------------------------------------------
 * Internal (physical) pixel formats
 *
 * M3G_NO_FORMAT must be zero: src/m3g_image.c:533 returns (M3GPixelFormat)0
 * for "invalid" and src/m3g_symbian_gl.cpp:51 returns M3G_NO_FORMAT for the
 * same case.
 *
 * NOTE: the relative order is inferred from the single ordering constraint
 * in the tree -- src/m3g_rendercontext.c:356,
 * `m3gInRange(format, M3G_RGB8, M3G_RGBA4)` decides whether a format can be
 * rendered into. Every colour format that is bound as a render target
 * (src/m3g_rendercontext.inl:57-82, :1497, src/m3g_symbian_gl.cpp:36-46,
 * src/m3g_image.c:541-545) therefore has to lie between M3G_RGB8 and
 * M3G_RGBA4 inclusive, and the greyscale/alpha and paletted formats have to
 * lie outside it. The exact numbering within those two groups is not
 * observable from the source.
 *--------------------------------------------------------------------*/

typedef enum {
    M3G_NO_FORMAT           = 0,

    /* Not renderable: greyscale and alpha-only */
    M3G_L8,                     /* src/m3g_image.c:793  */
    M3G_A8,                     /* src/m3g_image.c:794  */
    M3G_LA4,                    /* src/m3g_image.c:795  */
    M3G_LA8,                    /* src/m3g_image.c:804  */

    /* Renderable colour formats: [M3G_RGB8 .. M3G_RGBA4] */
    M3G_RGB8,                   /* src/m3g_rendercontext.c:356 (range low)  */
    M3G_RGB4,                   /* src/m3g_symbian_gl.cpp:38  */
    M3G_RGB565,                 /* src/m3g_rendercontext.inl:63 */
    M3G_RGB8_32,                /* src/m3g_symbian_gl.cpp:46  */
    M3G_BGR8_32,                /* src/m3g_symbian_gl.cpp:42  */
    M3G_RGBA8,                  /* src/m3g_rendercontext.inl:1497 */
    M3G_BGRA8,                  /* src/m3g_symbian_gl.cpp:45  */
    M3G_ARGB8,                  /* src/m3g_image.c:810  */
    M3G_RGB5A1,                 /* src/m3g_rendercontext.c:372 */
    M3G_RGBA4,                  /* src/m3g_rendercontext.c:356 (range high) */

    /* Not renderable: paletted */
    M3G_PALETTE8_RGB8,          /* src/m3g_image.c:796  */
    M3G_PALETTE8_RGB8_32,       /* src/m3g_image.c:797  */
    M3G_PALETTE8_RGBA8          /* src/m3g_image.c:798  */
} M3GPixelFormat;

/*----------------------------------------------------------------------
 * Image creation flags (M3Gbitmask, argument of m3gCreateImage)
 *
 * Documented at src/m3g_image.c:1213-1215; used at :1252-1262, :1383.
 * They are stored in the 8-bit field `M3Gbitmask flags : 8` at
 * inc/m3g_image.h:58 ("flags defined in m3g_core.h"), so all four bits must
 * fit in the low byte.
 *--------------------------------------------------------------------*/

#define M3G_STATIC              0x01u
#define M3G_DYNAMIC             0x02u
#define M3G_RENDERING_TARGET    0x04u
#define M3G_PALETTED            0x08u

/*----------------------------------------------------------------------
 * Rendering target buffer bits (m3gSetRenderBuffers)
 *
 * The accepted set is enumerated at src/m3g_rendercontext.c:1206;
 * the default is COLOR|DEPTH (src/m3g_rendercontext.c:1180).
 *
 * NOTE: the numeric values are not observable from the source -- nothing
 * passes them to GL directly. The OpenGL clear-mask bits are used here.
 *--------------------------------------------------------------------*/

#define M3G_COLOR_BUFFER_BIT        0x0001u
#define M3G_DEPTH_BUFFER_BIT        0x0002u
#define M3G_STENCIL_BUFFER_BIT      0x0004u
#define M3G_MULTISAMPLE_BUFFER_BIT  0x0008u

/*----------------------------------------------------------------------
 * Rendering hint bits (m3gSetRenderHints)
 *
 * The accepted set is enumerated at src/m3g_rendercontext.c:1226.
 *
 * NOTE: values taken from the javax.microedition.m3g.Graphics3D hint
 * constants (ANTIALIAS/DITHER/TRUE_COLOR/OVERWRITE); not otherwise
 * observable from the source.
 *--------------------------------------------------------------------*/

#define M3G_ANTIALIAS_BIT       0x0002u
#define M3G_DITHER_BIT          0x0004u
#define M3G_TRUECOLOR_BIT       0x0008u
#define M3G_OVERWRITE_BIT       0x0010u

/*----------------------------------------------------------------------
 * Camera projection types
 *
 * Byte read from the .m3g stream and switched on directly at
 * src/m3g_loader.c:1094-1114, so the values are the
 * javax.microedition.m3g.Camera constants. Also src/m3g_camera.c:56,:468,
 * :501, :527.
 *--------------------------------------------------------------------*/

#define M3G_GENERIC             48
#define M3G_PARALLEL            49
#define M3G_PERSPECTIVE         50

/*----------------------------------------------------------------------
 * Background image modes
 *
 * Bytes read from the stream and handed to m3gSetBgMode unchanged
 * (src/m3g_loader.c:1156); src/m3g_background.c:466 validates
 * M3G_BORDER <= mode <= M3G_REPEAT, so they are adjacent.
 * Values are the javax.microedition.m3g.Background constants.
 *--------------------------------------------------------------------*/

#define M3G_BORDER              32
#define M3G_REPEAT              33

/*----------------------------------------------------------------------
 * Fog modes
 *
 * Byte from the stream, src/m3g_loader.c:2089; src/m3g_fog.c:295.
 * Values are the javax.microedition.m3g.Fog constants.
 *--------------------------------------------------------------------*/

#define M3G_EXPONENTIAL_FOG     80
#define M3G_LINEAR_FOG          81

/*----------------------------------------------------------------------
 * Light modes
 *
 * Byte from the stream, src/m3g_loader.c:1795; src/m3g_light.c:366
 * validates M3G_AMBIENT <= mode <= M3G_SPOT, so the four are contiguous
 * in this order. Values are the javax.microedition.m3g.Light constants.
 *--------------------------------------------------------------------*/

#define M3G_AMBIENT             128
#define M3G_DIRECTIONAL         129
#define M3G_OMNI                130
#define M3G_SPOT                131

/*----------------------------------------------------------------------
 * Node alignment targets
 *
 * Bytes from the stream, src/m3g_loader.c:1036-1044; src/m3g_node.c:1182
 * validates m3gInRange(target, M3G_NONE, M3G_Z_AXIS), so all five are
 * contiguous with M3G_NONE lowest and M3G_Z_AXIS highest
 * (src/m3g_node.c:46-82). Values are the javax.microedition.m3g.Node
 * constants.
 *--------------------------------------------------------------------*/

#define M3G_NONE                144
#define M3G_ORIGIN              145
#define M3G_X_AXIS              146
#define M3G_Y_AXIS              147
#define M3G_Z_AXIS              148

/*----------------------------------------------------------------------
 * CompositingMode blending modes
 *
 * Byte from the stream, src/m3g_loader.c:1514; validated at
 * src/m3g_compositingmode.c:235, applied at :93-116.
 * Values are the javax.microedition.m3g.CompositingMode constants; the
 * core spells the ALPHA mode M3G_ALPHA_BLEND because M3G_ALPHA is already
 * taken by M3GImageFormat.
 *--------------------------------------------------------------------*/

#define M3G_ALPHA_BLEND         64
#define M3G_ALPHA_ADD           65
#define M3G_MODULATE            66
#define M3G_MODULATE_X2         67
#define M3G_REPLACE             68

/*----------------------------------------------------------------------
 * PolygonMode culling, shading and winding
 *
 * Bytes from the stream, src/m3g_loader.c:1546-1548; validated at
 * src/m3g_polygonmode.c:251-253, :286, :318.
 * Values are the javax.microedition.m3g.PolygonMode constants.
 *--------------------------------------------------------------------*/

#define M3G_CULL_BACK           160
#define M3G_CULL_FRONT          161
#define M3G_CULL_NONE           162

#define M3G_SHADE_FLAT          164
#define M3G_SHADE_SMOOTH        165

#define M3G_WINDING_CCW         168
#define M3G_WINDING_CW          169

/*----------------------------------------------------------------------
 * Texture2D filtering, wrapping and blending
 *
 * Bytes from the stream, src/m3g_loader.c:2235-2238; validated at
 * src/m3g_texture.c:459-464, :485-490, :529-534.
 * Values are the javax.microedition.m3g.Texture2D constants.
 *--------------------------------------------------------------------*/

#define M3G_FILTER_BASE_LEVEL   208
#define M3G_FILTER_LINEAR       209
#define M3G_FILTER_NEAREST      210

#define M3G_FUNC_ADD            224
#define M3G_FUNC_BLEND          225
#define M3G_FUNC_DECAL          226
#define M3G_FUNC_MODULATE       227
#define M3G_FUNC_REPLACE        228

#define M3G_WRAP_CLAMP          240
#define M3G_WRAP_REPEAT         241

/*----------------------------------------------------------------------
 * KeyframeSequence interpolation and repeat modes
 *
 * Read from the stream, src/m3g_loader.c:1857-1861;
 * src/m3g_keyframesequence.c:756 validates
 * M3G_LINEAR <= interpolation <= M3G_STEP (contiguous, in this order),
 * and :985 accepts only M3G_CONSTANT / M3G_LOOP.
 * Values are the javax.microedition.m3g.KeyframeSequence constants.
 *--------------------------------------------------------------------*/

#define M3G_LINEAR              176
#define M3G_SLERP               177
#define M3G_SPLINE              178
#define M3G_SQUAD               179
#define M3G_STEP                180

#define M3G_CONSTANT            192
#define M3G_LOOP                193

/*----------------------------------------------------------------------
 * Material colour target bits (m3gSetColor / m3gGetColor)
 *
 * Passed by the loader at src/m3g_loader.c:2049-2055 and combined into
 * ALL_TARGET_MASK at src/m3g_material.c:32, so each must be a distinct
 * single bit. Values are the javax.microedition.m3g.Material constants.
 *--------------------------------------------------------------------*/

#define M3G_AMBIENT_BIT         1024
#define M3G_DIFFUSE_BIT         2048
#define M3G_EMISSIVE_BIT        4096
#define M3G_SPECULAR_BIT        8192

/*----------------------------------------------------------------------
 * Animation target properties (m3gCreateAnimationTrack)
 *
 * 32-bit value read from the stream, src/m3g_loader.c:2008;
 * src/m3g_animationtrack.c:307 validates
 * M3G_ANIM_ALPHA <= property <= M3G_ANIM_VISIBILITY, so the whole set is
 * contiguous with ALPHA first and VISIBILITY last -- which matches the
 * alphabetical javax.microedition.m3g.AnimationTrack constants 256..276.
 * Component counts per property: src/m3g_animationtrack.c:155-181.
 *--------------------------------------------------------------------*/

#define M3G_ANIM_ALPHA              256
#define M3G_ANIM_AMBIENT_COLOR      257
#define M3G_ANIM_COLOR              258
#define M3G_ANIM_CROP               259
#define M3G_ANIM_DENSITY            260
#define M3G_ANIM_DIFFUSE_COLOR      261
#define M3G_ANIM_EMISSIVE_COLOR     262
#define M3G_ANIM_FAR_DISTANCE       263
#define M3G_ANIM_FIELD_OF_VIEW      264
#define M3G_ANIM_INTENSITY          265
#define M3G_ANIM_MORPH_WEIGHTS      266
#define M3G_ANIM_NEAR_DISTANCE      267
#define M3G_ANIM_ORIENTATION        268
#define M3G_ANIM_PICKABILITY        269
#define M3G_ANIM_SCALE              270
#define M3G_ANIM_SHININESS          271
#define M3G_ANIM_SPECULAR_COLOR     272
#define M3G_ANIM_SPOT_ANGLE         273
#define M3G_ANIM_SPOT_EXPONENT      274
#define M3G_ANIM_TRANSLATION        275
#define M3G_ANIM_VISIBILITY         276

/*----------------------------------------------------------------------
 * Property selectors for the combined getters/setters
 *
 * These are private to this API (they have no JSR-184 counterpart); the
 * implementation only requires the members of each switch to be distinct.
 * Referenced from the switches listed beside each group.
 *
 * NOTE: numeric values inferred; the only hard constraint is that
 * M3G_GET_TEXCOORDS0 + 1 must not collide with any other vertex-buffer
 * selector (src/m3g_vertexbuffer.c:974).
 *--------------------------------------------------------------------*/

/* m3gGetVertexArray -- src/m3g_vertexbuffer.c:956-974 */
#define M3G_GET_POSITIONS       0
#define M3G_GET_NORMALS         1
#define M3G_GET_COLORS          2
#define M3G_GET_TEXCOORDS0      3   /* +1 selects texture unit 1 */

/* m3gGetBgMode -- src/m3g_background.c:586-590 */
#define M3G_GET_MODEX           0
#define M3G_GET_MODEY           1

/* m3gGetBgCrop, m3gGetCrop -- src/m3g_background.c:613-621, src/m3g_sprite.c:1000-1010 */
#define M3G_GET_CROPX           0
#define M3G_GET_CROPY           1
#define M3G_GET_CROPWIDTH       2
#define M3G_GET_CROPHEIGHT      3

/* m3gGetFogDistance -- src/m3g_fog.c:353-358 */
#define M3G_GET_NEAR            0
#define M3G_GET_FAR             1

/* m3gGetAttenuation -- src/m3g_light.c:524-530 */
#define M3G_GET_CONSTANT        0
#define M3G_GET_LINEAR          1
#define M3G_GET_QUADRATIC       2

/* m3gSetBgEnable / m3gIsBgEnabled -- src/m3g_background.c:641-670 */
#define M3G_SETGET_COLORCLEAR   0
#define M3G_SETGET_DEPTHCLEAR   1

/* m3gEnable / m3gIsEnabled -- src/m3g_node.c:1275-1310 */
#define M3G_SETGET_RENDERING    0
#define M3G_SETGET_PICKING      1

/*----------------------------------------------------------------------
 * Statistics and profiling counters (m3gGetStatistic)
 *
 * Indexes into Interface::statistics[M3G_STAT_MAX] (src/m3g_interface.c:117)
 * and into M3GLogger::tickCount[M3G_STAT_MAX] (src/m3g_symbian.cpp:36).
 * src/m3g_interface.c:1859-1864 requires 0 <= stat < M3G_STAT_MAX and clears
 * every counter below M3G_STAT_CUMULATIVE after it is read, so
 * M3G_STAT_CUMULATIVE is a partition marker: per-frame counters below it,
 * running totals at or above it.
 *
 * NOTE: which individual counters belong on which side of
 * M3G_STAT_CUMULATIVE is inferred from what each one measures. The
 * memory/object/peak and cache-load counters track live state and must not
 * be cleared (src/m3g_interface.c:1516-1531 compares them across frames);
 * the timers and per-frame event counters are reset each time.
 *--------------------------------------------------------------------*/

typedef enum {
    /* Profiling timers -- reset when queried */
    M3G_PROFILE_ANIM = 0,               /* src/m3g_object.c:672 */
    M3G_PROFILE_COMMIT,                 /* src/m3g_rendercontext.c:1755 */
    M3G_PROFILE_LOADER_DECODE,          /* src/m3g_loader.c:2890 */
    M3G_PROFILE_MORPH,                  /* src/m3g_morphingmesh.c:124 */
    M3G_PROFILE_NGL_DRAW,               /* src/m3g_indexbuffer.c:74 */
    M3G_PROFILE_PICK,                   /* src/m3g_group.c:835 */
    M3G_PROFILE_SETUP,                  /* src/m3g_rendercontext.c:1747 */
    M3G_PROFILE_SETUP_SORT,             /* src/m3g_renderqueue.c:249 */
    M3G_PROFILE_SETUP_TRANSFORMS,       /* src/m3g_camera.c:273 */
    M3G_PROFILE_SKIN,                   /* src/m3g_skinnedmesh.c:1343 */
    M3G_PROFILE_TCACHE,                 /* src/m3g_tcache.c:177 */
    M3G_PROFILE_TRANSFORM_INVERT,       /* src/m3g_group.c:969 */
    M3G_PROFILE_TRANSFORM_TO,           /* src/m3g_node.c:1049 */
    M3G_PROFILE_VALIDATE,               /* src/m3g_rendercontext.c:1727 */
    M3G_PROFILE_VFC_TEST,               /* src/m3g_node.c:956 */
    M3G_PROFILE_VFC_UPDATE,             /* src/m3g_node.c:767 */

    /* Per-frame event counters -- reset when queried */
    M3G_STAT_CULLING_TESTS,             /* src/m3g_node.c:991 */
    M3G_STAT_MEMORY_ALLOCS,             /* src/m3g_interface.c:534 */
    M3G_STAT_MEMORY_LOCKS,              /* src/m3g_interface.c:430 */
    M3G_STAT_RENDERQUEUE_SIZE,          /* src/m3g_renderqueue.c:112 */
    M3G_STAT_RENDER_NODES,              /* src/m3g_mesh.c:118 */
    M3G_STAT_RENDER_NODES_CULLED,       /* src/m3g_mesh.c:131 */
    M3G_STAT_RENDER_NODES_DRAWN,
    M3G_STAT_TCACHE_COMPOSITE_COLLISIONS,   /* src/m3g_tcache.c:154 */
    M3G_STAT_TCACHE_COMPOSITE_HITS,         /* src/m3g_tcache.c:241 */
    M3G_STAT_TCACHE_COMPOSITE_INSERTS,      /* src/m3g_tcache.c:162 */
    M3G_STAT_TCACHE_COMPOSITE_MISSES,       /* src/m3g_tcache.c:244 */
    M3G_STAT_TCACHE_PATH_COLLISIONS,        /* src/m3g_tcache.c:218 */
    M3G_STAT_TCACHE_PATH_FLUSHES,           /* src/m3g_tcache.c:183 */
    M3G_STAT_TCACHE_PATH_HITS,              /* src/m3g_tcache.c:282 */
    M3G_STAT_TCACHE_PATH_INSERTS,           /* src/m3g_tcache.c:229 */
    M3G_STAT_TCACHE_PATH_MISSES,            /* src/m3g_tcache.c:288 */

    /* Running totals -- never cleared (src/m3g_interface.c:1862) */
    M3G_STAT_CUMULATIVE,
    M3G_STAT_BOUNDING_BOXES = M3G_STAT_CUMULATIVE,  /* src/m3g_group.c:124 */
    M3G_STAT_MEMORY_ALLOCATED,          /* src/m3g_interface.c:538 */
    M3G_STAT_MEMORY_MALLOC_BYTES,       /* src/m3g_interface.c:539 */
    M3G_STAT_MEMORY_MALLOC_PEAK,        /* src/m3g_interface.c:1501 */
    M3G_STAT_MEMORY_OBJECT_BYTES,       /* src/m3g_interface.c:807 */
    M3G_STAT_MEMORY_OBJECT_PEAK,        /* src/m3g_interface.c:1502 */
    M3G_STAT_MEMORY_PEAK,               /* src/m3g_interface.c:1503 */
    M3G_STAT_OBJECTS,                   /* src/m3g_object.c:62 */
    M3G_STAT_RENDERABLES,               /* src/m3g_mesh.c:470 */
    M3G_STAT_TCACHE_COMPOSITE_LOAD,     /* src/m3g_tcache.c:157 */
    M3G_STAT_TCACHE_PATH_LOAD,          /* src/m3g_tcache.c:221 */

    M3G_STAT_MAX
} M3Gstatistic;

/*----------------------------------------------------------------------
 * Application callbacks
 *
 * These are function *types*, not pointer types: the implementation
 * declares the members that hold them as `m3gMallocFunc *malloc` etc.
 * (src/m3g_interface.c:60-68).
 *--------------------------------------------------------------------*/

/*!
 * \brief Heap allocation; src/m3g_interface.c:503, :1613
 *
 * NOTE: the argument is M3Guint rather than M3Gsize because M3Gsize is not
 * declared until inc/m3g_defs.h:507/510, i.e. after this header is included
 * (m3g_defs.h:28). Every call site passes INSTRUMENTATED_SIZE(...), which
 * yields M3Guint (src/m3g_interface.c:176, :192).
 */
typedef void *m3gMallocFunc(M3Guint bytes);

/*! \brief Heap release; src/m3g_interface.c:646, :1756 */
typedef void m3gFreeFunc(void *ptr);

/*! \brief Movable-object allocation; src/m3g_interface.c:780 */
typedef M3GMemObject m3gObjectAllocator(M3Guint bytes);

/*! \brief Movable-object locking; src/m3g_interface.c:230, :1051 */
typedef void *m3gObjectResolver(M3GMemObject handle);

/*! \brief Movable-object release; src/m3g_interface.c:894 */
typedef void m3gObjectDeallocator(M3GMemObject handle);

/*! \brief Error callback; src/m3g_interface.c:1200, :1220 */
typedef void m3gErrorHandler(M3Genum errorCode, M3GInterface m3g);

/*! \brief Frame buffer lock callback; src/m3g_interface.c:1123 */
typedef void *m3gBeginRenderFunc(M3Guint userTargetHandle);

/*! \brief Frame buffer release callback; src/m3g_interface.c:1144 */
typedef void m3gEndRenderFunc(M3Guint userTargetHandle);

/*!
 * \brief Rendering target release callback; src/m3g_interface.c:1164
 *
 * NOTE: the type is required by the Interface member declared at
 * src/m3g_interface.c:68, but nothing in the drop ever installs one, so
 * M3Gparams below has no corresponding field.
 */
typedef void m3gReleaseTargetFunc(M3Guint userTargetHandle);

/*----------------------------------------------------------------------
 * Interface creation parameters
 *
 * Exactly the fields read by m3gCreateInterface, src/m3g_interface.c:1597-1646.
 * mallocFunc and freeFunc are mandatory (:1597-1599); objAllocFunc,
 * objResolveFunc and objFreeFunc must be supplied together or not at all
 * (:1602-1604), in which case malloc/free are used with an identity
 * resolver (:1637-1639). errorFunc, beginRenderFunc, endRenderFunc and
 * userContext may all be NULL (:1122, :1143, :1199).
 *--------------------------------------------------------------------*/

typedef struct {
    m3gMallocFunc           *mallocFunc;
    m3gFreeFunc             *freeFunc;
    m3gObjectAllocator      *objAllocFunc;
    m3gObjectResolver       *objResolveFunc;
    m3gObjectDeallocator    *objFreeFunc;
    m3gErrorHandler         *errorFunc;
    m3gBeginRenderFunc      *beginRenderFunc;
    m3gEndRenderFunc        *endRenderFunc;
    void                    *userContext;
} M3Gparams;

/*----------------------------------------------------------------------
 * Public API
 *
 * One prototype per function defined with the M3G_API qualifier in src/,
 * plus the five public entry points whose definitions omit it (noted in
 * their groups). Signatures are taken verbatim from the definitions, with
 * the Splint annotations stripped and the internal aliases Matrix/Quat/
 * Vec3/Vec4 spelled out as M3GMatrix/M3GQuat/M3GVec3/M3GVec4
 * (inc/m3g_defs.h:844-848).
 *--------------------------------------------------------------------*/

/* Interface (m3g_interface.c) */

M3G_API M3GInterface m3gCreateInterface( const M3Gparams *params);
M3G_API void m3gDeleteInterface(M3GInterface interface);
M3G_API M3Genum m3gGetError(M3GInterface interface);
M3G_API void *m3gGetUserContext(M3GInterface interface);
M3G_API M3Gbool m3gIsAntialiasingSupported(M3GInterface interface);
M3G_API void m3gGarbageCollect(M3GInterface interface);
M3G_API M3Gint m3gGetStatistic(M3GInterface hInterface, M3Gstatistic stat);

/* Object -- common base class (m3g_object.c) */

M3G_API void m3gDeleteObject(M3GObject hObject);
M3G_API void m3gAddRef(M3GObject hObject);
M3G_API void m3gDeleteRef(M3GObject hObject);
M3G_API M3GClass m3gGetClass(M3GObject hObject);
M3G_API M3GInterface m3gGetObjectInterface(M3GObject hObject);
M3G_API M3Gint m3gAddAnimationTrack(M3GObject hObject, M3GAnimationTrack hAnimationTrack);
M3G_API void m3gRemoveAnimationTrack(M3GObject hObject, M3GAnimationTrack hAnimationTrack);
M3G_API M3Gint m3gGetAnimationTrackCount(M3GObject hObject);
M3G_API M3GAnimationTrack m3gGetAnimationTrack(M3GObject hObject, M3Gint idx);
M3G_API M3Gint m3gAnimate(M3GObject hObject, M3Gint time);
M3G_API void m3gSetUserID(M3GObject hObject, M3Gint userID);
M3G_API M3Gint m3gGetUserID(M3GObject hObject);
M3G_API M3GObject m3gDuplicate(M3GObject hObject, M3GObject *hReferences);
M3G_API M3Gint m3gGetReferences(M3GObject hObject, M3GObject *references, M3Gint length);
M3G_API M3GObject m3gFind(M3GObject hObject, M3Gint userID);

/* Transformable (m3g_transformable.c) */

M3G_API void m3gSetOrientation(M3GTransformable handle, M3Gfloat angle, M3Gfloat ax, M3Gfloat ay, M3Gfloat az);
M3G_API void m3gPostRotate(M3GTransformable handle, M3Gfloat angle, M3Gfloat ax, M3Gfloat ay, M3Gfloat az);
M3G_API void m3gPreRotate(M3GTransformable handle, M3Gfloat angle, M3Gfloat ax, M3Gfloat ay, M3Gfloat az);
M3G_API void m3gGetOrientation(M3GTransformable handle, M3Gfloat *angleAxis);
M3G_API void m3gSetScale(M3GTransformable handle, M3Gfloat sx, M3Gfloat sy, M3Gfloat sz);
M3G_API void m3gScale(M3GTransformable handle, M3Gfloat sx, M3Gfloat sy, M3Gfloat sz);
M3G_API void m3gGetScale(M3GTransformable handle, M3Gfloat *scale);
M3G_API void m3gSetTranslation(M3GTransformable handle, M3Gfloat tx, M3Gfloat ty, M3Gfloat tz);
M3G_API void m3gTranslate(M3GTransformable handle, M3Gfloat tx, M3Gfloat ty, M3Gfloat tz);
M3G_API void m3gGetTranslation(M3GTransformable handle, M3Gfloat *translation);
M3G_API void m3gSetTransform(M3GTransformable handle, const M3GMatrix *transform);
M3G_API void m3gGetTransform(M3GTransformable handle, M3GMatrix *transform);
M3G_API void m3gGetCompositeTransform(M3GTransformable handle, M3GMatrix *transform);

/* Node (m3g_node.c) */

M3G_API M3Gbool m3gGetTransformTo(M3GNode handle, M3GNode hTarget, M3GMatrix *transform);
M3G_API void m3gSetAlignment(M3GNode handle, M3GNode hZReference, M3Gint zTarget, M3GNode hYReference, M3Gint yTarget);
M3G_API void m3gAlignNode(M3GNode hNode, M3GNode hRef);
M3G_API void m3gSetAlphaFactor(M3GNode handle, M3Gfloat alphaFactor);
M3G_API M3Gfloat m3gGetAlphaFactor(M3GNode handle);
M3G_API void m3gEnable(M3GNode handle, M3Gint which, M3Gbool enable);
M3G_API M3Gint m3gIsEnabled(M3GNode handle, M3Gint which);
M3G_API void m3gSetScope(M3GNode handle, M3Gint id);
M3G_API M3Gint m3gGetScope(M3GNode handle);
M3G_API M3GNode m3gGetParent(M3GNode handle);
M3G_API M3GNode m3gGetZRef(M3GNode handle);
M3G_API M3GNode m3gGetYRef(M3GNode handle);
M3G_API M3Gint m3gGetAlignmentTarget(M3GNode handle, M3Gint axis);
M3G_API M3Gint m3gGetSubtreeSize(M3GNode handle);

/* Group (m3g_group.c) */

M3G_API M3GGroup m3gCreateGroup(M3GInterface interface);
M3G_API void m3gAddChild(M3GGroup handle, M3GNode hNode);
M3G_API void m3gRemoveChild(M3GGroup handle, M3GNode hNode);
M3G_API M3GNode m3gPick3D(M3GGroup handle, M3Gint mask, M3Gfloat *ray, M3Gfloat *result);
M3G_API M3GNode m3gPick2D(M3GGroup handle, M3Gint mask, M3Gfloat x, M3Gfloat y, M3GCamera hCamera, M3Gfloat *result);
M3G_API M3GNode m3gGetChild(M3GGroup handle, M3Gint idx);
M3G_API M3Gint m3gGetChildCount(M3GGroup handle);

/* World (m3g_world.c) */

M3G_API M3GWorld m3gCreateWorld(M3GInterface interface);
M3G_API void m3gSetActiveCamera(M3GWorld handle, M3GCamera hCamera);
M3G_API void m3gSetBackground(M3GWorld handle, M3GBackground hBackground);
M3G_API M3GBackground m3gGetBackground(M3GWorld handle);
M3G_API M3GCamera m3gGetActiveCamera(M3GWorld handle);

/* Camera (m3g_camera.c) */

M3G_API M3GCamera m3gCreateCamera(M3GInterface interface);
M3G_API void m3gSetParallel(M3GCamera handle, M3Gfloat height, M3Gfloat aspectRatio, M3Gfloat clipNear, M3Gfloat clipFar);
M3G_API void m3gSetPerspective(M3GCamera handle, M3Gfloat fovy, M3Gfloat aspectRatio, M3Gfloat clipNear, M3Gfloat clipFar);
M3G_API void m3gSetProjectionMatrix(M3GCamera handle, const M3GMatrix *transform);
M3G_API M3Gint m3gGetProjectionAsMatrix(M3GCamera handle, M3GMatrix *transform);
M3G_API M3Gint m3gGetProjectionAsParams(M3GCamera handle, M3Gfloat *params);

/* Light (m3g_light.c) */

M3G_API M3GLight m3gCreateLight(M3GInterface interface);
M3G_API void m3gSetIntensity(M3GLight handle, M3Gfloat intensity);
M3G_API void m3gSetLightColor(M3GLight handle, M3Guint rgb);
M3G_API void m3gSetLightMode(M3GLight handle, M3Gint mode);
M3G_API void m3gSetSpotAngle(M3GLight handle, M3Gfloat angle);
M3G_API void m3gSetSpotExponent(M3GLight handle, M3Gfloat exponent);
M3G_API void m3gSetAttenuation(M3GLight handle, M3Gfloat constant, M3Gfloat linear, M3Gfloat quadratic);
M3G_API M3Gfloat m3gGetIntensity(M3GLight handle);
M3G_API M3Guint m3gGetLightColor(M3GLight handle);
M3G_API M3Gint m3gGetLightMode(M3GLight handle);
M3G_API M3Gfloat m3gGetSpotAngle(M3GLight handle);
M3G_API M3Gfloat m3gGetSpotExponent(M3GLight handle);
M3G_API M3Gfloat m3gGetAttenuation(M3GLight handle, M3Gint type);

/* Background (m3g_background.c) */

M3G_API M3GBackground m3gCreateBackground(M3GInterface interface);
M3G_API void m3gSetBgColor(M3GBackground handle, M3Guint ARGB);
M3G_API void m3gSetBgMode(M3GBackground handle, M3Gint modeX, M3Gint modeY);
M3G_API void m3gSetBgCrop(M3GBackground handle, M3Gint cropX, M3Gint cropY, M3Gint width, M3Gint height);
M3G_API void m3gSetBgImage(M3GBackground handle, M3GImage hImage);
M3G_API M3GImage m3gGetBgImage(M3GBackground handle);
M3G_API M3Guint m3gGetBgColor(M3GBackground handle);
M3G_API M3Gint m3gGetBgMode(M3GBackground handle, M3Gint which);
M3G_API M3Gint m3gGetBgCrop(M3GBackground handle, M3Gint which);
M3G_API void m3gSetBgEnable(M3GBackground handle, M3Gint which, M3Gbool enable);
M3G_API M3Gbool m3gIsBgEnabled(M3GBackground handle, M3Gint which);

/* Fog (m3g_fog.c) */

M3G_API M3GFog m3gCreateFog(M3GInterface interface);
M3G_API void m3gSetFogMode(M3GFog handle, M3Gint mode);
M3G_API M3Gint m3gGetFogMode(M3GFog handle);
M3G_API void m3gSetFogLinear(M3GFog handle, M3Gfloat fogNear, M3Gfloat fogFar);
M3G_API M3Gfloat m3gGetFogDistance(M3GFog handle, M3Gint which);
M3G_API void m3gSetFogDensity(M3GFog handle, M3Gfloat density);
M3G_API M3Gfloat m3gGetFogDensity(M3GFog handle);
M3G_API void m3gSetFogColor(M3GFog handle, M3Guint rgb);
M3G_API M3Guint m3gGetFogColor(M3GFog handle);

/* Material (m3g_material.c) */

M3G_API M3GMaterial m3gCreateMaterial(M3GInterface interface);
M3G_API void m3gSetColor(M3GMaterial hMaterial, M3Genum target, M3Guint ARGB);
M3G_API M3Guint m3gGetColor(M3GMaterial hMaterial, M3Genum target);
M3G_API void m3gSetShininess(M3GMaterial hMaterial, M3Gfloat shininess);
M3G_API M3Gfloat m3gGetShininess(M3GMaterial hMaterial);
M3G_API void m3gSetVertexColorTrackingEnable(M3GMaterial hMaterial, M3Gbool enable);
M3G_API M3Gbool m3gIsVertexColorTrackingEnabled(M3GMaterial hMaterial);

/* CompositingMode (m3g_compositingmode.c) */

M3G_API M3GCompositingMode m3gCreateCompositingMode(M3GInterface interface);
M3G_API void m3gSetBlending(M3GCompositingMode handle, M3Gint mode);
M3G_API M3Gint m3gGetBlending(M3GCompositingMode handle);
M3G_API void m3gSetAlphaThreshold(M3GCompositingMode handle, M3Gfloat threshold);
M3G_API M3Gfloat m3gGetAlphaThreshold(M3GCompositingMode handle);
M3G_API void m3gEnableDepthTest(M3GCompositingMode handle, M3Gbool enable);
M3G_API void m3gEnableDepthWrite(M3GCompositingMode handle, M3Gbool enable);
M3G_API void m3gEnableColorWrite(M3GCompositingMode handle, M3Gbool enable);
M3G_API void m3gSetDepthOffset(M3GCompositingMode handle, M3Gfloat factor, M3Gfloat units);
M3G_API M3Gfloat m3gGetDepthOffsetFactor(M3GCompositingMode handle);
M3G_API M3Gfloat m3gGetDepthOffsetUnits(M3GCompositingMode handle);
M3G_API M3Gbool m3gIsAlphaWriteEnabled(M3GCompositingMode handle);
M3G_API M3Gbool m3gIsColorWriteEnabled(M3GCompositingMode handle);
M3G_API M3Gbool m3gIsDepthTestEnabled(M3GCompositingMode handle);
M3G_API M3Gbool m3gIsDepthWriteEnabled(M3GCompositingMode handle);
M3G_API void m3gSetAlphaWriteEnable(M3GCompositingMode handle, M3Gbool enable);

/* PolygonMode (m3g_polygonmode.c) */

M3G_API M3GPolygonMode m3gCreatePolygonMode(M3GInterface interface);
M3G_API void m3gSetLocalCameraLightingEnable(M3GPolygonMode handle, M3Gbool enable);
M3G_API M3Gint m3gGetCulling(M3GPolygonMode handle);
M3G_API void m3gSetWinding(M3GPolygonMode handle, M3Gint mode);
M3G_API M3Gint m3gGetWinding(M3GPolygonMode handle);
M3G_API void m3gSetShading(M3GPolygonMode handle, M3Gint mode);
M3G_API M3Gint m3gGetShading(M3GPolygonMode handle);
M3G_API void m3gSetTwoSidedLightingEnable(M3GPolygonMode handle, M3Gbool enable);
M3G_API M3Gbool m3gIsTwoSidedLightingEnabled(M3GPolygonMode handle);
M3G_API M3Gbool m3gIsLocalCameraLightingEnabled(M3GPolygonMode handle);
M3G_API M3Gbool m3gIsPerspectiveCorrectionEnabled(M3GPolygonMode handle);
M3G_API void m3gSetCulling(M3GPolygonMode handle, M3Gint mode);
M3G_API void m3gSetPerspectiveCorrectionEnable(M3GPolygonMode handle, M3Gbool enable);

/* Texture2D (m3g_texture.c) */

M3G_API M3GTexture m3gCreateTexture(M3GInterface interface, M3GImage hImage);
M3G_API void m3gSetTextureImage(M3GTexture hTexture, M3GImage hImage);
M3G_API M3GImage m3gGetTextureImage(M3GTexture hTexture);
M3G_API void m3gSetFiltering(M3GTexture hTexture, M3Gint levelFilter, M3Gint imageFilter);
M3G_API void m3gSetWrapping(M3GTexture hTexture, M3Gint wrapS, M3Gint wrapT);
M3G_API M3Gint m3gGetWrappingS(M3GTexture hTexture);
M3G_API M3Gint m3gGetWrappingT(M3GTexture hTexture);
M3G_API void m3gTextureSetBlending(M3GTexture hTexture, M3Gint func);
M3G_API M3Gint m3gTextureGetBlending(M3GTexture hTexture);
M3G_API void m3gSetBlendColor(M3GTexture hTexture, M3Guint RGB);
M3G_API M3Guint m3gGetBlendColor(M3GTexture hTexture);
M3G_API void m3gGetFiltering(M3GTexture hTexture, M3Gint *levelFilter, M3Gint *imageFilter);

/* Appearance (m3g_appearance.c) */

M3G_API M3GAppearance m3gCreateAppearance(M3GInterface hInterface);
M3G_API M3GCompositingMode m3gGetCompositingMode(M3GAppearance hAppearance);
M3G_API M3GFog m3gGetFog(M3GAppearance hAppearance);
M3G_API M3GMaterial m3gGetMaterial(M3GAppearance hAppearance);
M3G_API M3GPolygonMode m3gGetPolygonMode(M3GAppearance hAppearance);
M3G_API M3GTexture m3gGetTexture(M3GAppearance hAppearance, M3Gint unit);
M3G_API M3Gint m3gGetLayer(M3GAppearance hAppearance);
M3G_API void m3gSetCompositingMode(M3GAppearance hAppearance, M3GCompositingMode hMode);
M3G_API void m3gSetPolygonMode(M3GAppearance hAppearance, M3GPolygonMode hMode);
M3G_API void m3gSetLayer(M3GAppearance hAppearance, M3Gint layer);
M3G_API void m3gSetMaterial(M3GAppearance hAppearance, M3GMaterial hMaterial);
M3G_API void m3gSetTexture(M3GAppearance hAppearance, M3Gint unit, M3GTexture hTexture);
M3G_API void m3gSetFog(M3GAppearance hAppearance, M3GFog hFog);

/* Mesh (m3g_mesh.c) */

M3G_API M3GMesh m3gCreateMesh(M3GInterface interface, M3GVertexBuffer hVertices, M3GIndexBuffer *hTriangles, M3GAppearance *hAppearances, M3Gint trianglePatchCount);
M3G_API void m3gSetAppearance(M3GMesh handle, M3Gint appearanceIndex, M3GAppearance hAppearance);
M3G_API M3GAppearance m3gGetAppearance(M3GMesh handle, M3Gint idx);
M3G_API M3GIndexBuffer m3gGetIndexBuffer(M3GMesh handle, M3Gint idx);
M3G_API M3GVertexBuffer m3gGetVertexBuffer(M3GMesh handle);
M3G_API M3Gint m3gGetSubmeshCount(M3GMesh handle);

/* MorphingMesh (m3g_morphingmesh.c) */

M3G_API M3GMorphingMesh m3gCreateMorphingMesh(M3GInterface interface, M3GVertexBuffer hVertices, M3GVertexBuffer *hTargets, M3GIndexBuffer *hTriangles, M3GAppearance *hAppearances, M3Gint trianglePatchCount, M3Gint targetCount);
M3G_API void m3gSetWeights(M3GMorphingMesh handle, M3Gfloat *weights, M3Gint numWeights);
M3G_API void m3gGetWeights(M3GMorphingMesh handle, M3Gfloat *weights, M3Gint numWeights);
M3G_API M3GVertexBuffer m3gGetMorphTarget(M3GMorphingMesh handle, M3Gint idx);
M3G_API M3Gint m3gGetMorphTargetCount(M3GMorphingMesh handle);

/* SkinnedMesh (m3g_skinnedmesh.c) */

M3G_API M3GSkinnedMesh m3gCreateSkinnedMesh(M3GInterface interface, M3GVertexBuffer hVertices, M3GIndexBuffer *hTriangles, M3GAppearance *hAppearances, M3Gint trianglePatchCount, M3GGroup hSkeleton);
M3G_API void m3gAddTransform(M3GSkinnedMesh handle, M3GNode hNode, M3Gint weight, M3Gint firstVertex, M3Gint numVertices);
M3G_API M3GGroup m3gGetSkeleton(M3GSkinnedMesh handle);
M3G_API void m3gGetBoneTransform(M3GSkinnedMesh handle, M3GNode hBone, M3GMatrix *transform);
M3G_API M3Gint m3gGetBoneVertices(M3GSkinnedMesh handle, M3GNode hBone, M3Gint *indices, M3Gfloat *weights);

/* Sprite3D (m3g_sprite.c) */

M3G_API M3GSprite m3gCreateSprite(M3GInterface hInterface, M3Gbool scaled, M3GImage hImage, M3GAppearance hAppearance);
M3G_API M3Gbool m3gIsScaledSprite(M3GSprite handle);
M3G_API void m3gSetSpriteAppearance(M3GSprite handle, M3GAppearance hAppearance);
M3G_API M3Gbool m3gSetSpriteImage(M3GSprite handle, M3GImage hImage);
M3G_API void m3gSetCrop(M3GSprite handle, M3Gint cropX, M3Gint cropY, M3Gint width, M3Gint height);
M3G_API M3GAppearance m3gGetSpriteAppearance(M3GSprite handle);
M3G_API M3GImage m3gGetSpriteImage(M3GSprite handle);
M3G_API M3Gint m3gGetCrop(M3GSprite handle, M3Gint which);

/* VertexArray (m3g_vertexarray.c) */

M3G_API M3GVertexArray m3gCreateVertexArray(M3GInterface interface, M3Gsizei count, M3Gint size, M3Gdatatype type);
M3G_API void m3gGetVertexArrayParams(M3GVertexArray handle, M3Gsizei *count, M3Gint *size, M3Gdatatype *type, M3Gsizei *stride);
M3G_API void *m3gMapVertexArray(M3GVertexArray handle);
M3G_API const void *m3gMapVertexArrayReadOnly(M3GVertexArray handle);
M3G_API void m3gUnmapVertexArray(M3GVertexArray handle);
M3G_API void m3gSetVertexArrayElements(M3GVertexArray handle, M3Gint first, M3Gsizei count, M3Gsizei srcLength, M3Gdatatype type, const void *src);
M3G_API void m3gGetVertexArrayElements(M3GVertexArray handle, M3Gint first, M3Gsizei count, M3Gsizei dstLength, M3Gdatatype type, void *dst);
M3G_API void m3gTransformArray(M3GVertexArray handle, M3GMatrix *transform, M3Gfloat *out, M3Gint outLength, M3Gbool w);

/* VertexBuffer (m3g_vertexbuffer.c) */

M3G_API M3GVertexBuffer m3gCreateVertexBuffer(M3GInterface interface);
M3G_API void m3gSetColorArray(M3GVertexBuffer hBuffer, M3GVertexArray hArray);
M3G_API void m3gSetNormalArray(M3GVertexBuffer hBuffer, M3GVertexArray hArray);
M3G_API void m3gSetTexCoordArray(M3GVertexBuffer hBuffer, M3Gint unit, M3GVertexArray hArray, M3Gfloat scale, M3Gfloat *bias, M3Gint biasLength);
M3G_API void m3gSetVertexArray(M3GVertexBuffer hBuffer, M3GVertexArray hArray, M3Gfloat scale, M3Gfloat *bias, M3Gint biasLength);
M3G_API void m3gSetVertexDefaultColor(M3GVertexBuffer handle, M3Guint ARGB);
M3G_API M3Guint m3gGetVertexDefaultColor(M3GVertexBuffer handle);
M3G_API M3GVertexArray m3gGetVertexArray(M3GVertexBuffer handle, M3Gint which, M3Gfloat *scaleBias, M3Gint sbLength);
M3G_API M3Gint m3gGetVertexCount(M3GVertexBuffer handle);

/* IndexBuffer (m3g_indexbuffer.c) */

M3G_API M3GIndexBuffer m3gCreateImplicitStripBuffer( M3GInterface interface, M3Gsizei stripCount, const M3Gsizei *stripLengths, M3Gint firstIndex);
M3G_API M3GIndexBuffer m3gCreateStripBuffer(M3GInterface interface, M3Gprimitive primitive, M3Gsizei stripCount, const M3Gsizei *stripLengths, M3Gdatatype type, M3Gsizei numIndices, const void *stripIndices);
M3G_API M3Gint m3gGetBatchCount(M3GIndexBuffer buffer);
M3G_API M3Gbool m3gGetBatchIndices(M3GIndexBuffer buffer, M3Gint batchIndex, M3Gint *indices);
M3G_API M3Gint m3gGetBatchSize(M3GIndexBuffer buffer, M3Gint batchIndex);
M3G_API M3Gprimitive m3gGetPrimitive(M3GIndexBuffer buffer);

/* Image2D (m3g_image.c) */

M3G_API M3GImage m3gCreateImage( M3GInterface interface, M3GImageFormat srcFormat, M3Gint width, M3Gint height, M3Gbitmask flags);
M3G_API void m3gCommitImage(M3GImage hImage);
M3G_API M3Gbool m3gIsMutable(M3GImage hImage);
M3G_API M3GImageFormat m3gGetFormat(M3GImage hImage);
M3G_API M3Gint m3gGetWidth(M3GImage hImage);
M3G_API M3Gint m3gGetHeight(M3GImage hImage);
M3G_API void m3gSetImage(M3GImage hImage, const void *srcPixels);
M3G_API void m3gGetImageARGB(M3GImage hImage, M3Guint *pixels);
M3G_API void m3gSetImagePalette(M3GImage hImage, M3Gint paletteLength, const void *srcPalette);
M3G_API void m3gSetImageScanline(M3GImage hImage, M3Gint line, M3Gbool trueAlpha, const M3Guint *pixels);
M3G_API void m3gSetSubImage(M3GImage hImage, M3Gint x, M3Gint y, M3Gint width, M3Gint height, M3Gint length, const void *pixels);

/* RenderContext / Graphics3D (m3g_rendercontext.c, m3g_rendercontext.inl) */

M3G_API void m3gSetAlphaWrite(M3GRenderContext ctx, M3Gbool enable);
M3G_API M3Gbool m3gGetAlphaWrite(M3GRenderContext ctx);
M3G_API void m3gFreeGLESResources(M3GRenderContext ctx);
M3G_API M3GRenderContext m3gCreateContext(M3GInterface interface);
M3G_API M3Gbool m3gSetRenderBuffers(M3GRenderContext hCtx, M3Gbitmask bufferBits);
M3G_API M3Gbool m3gSetRenderHints(M3GRenderContext hCtx, M3Gbitmask modeBits);
M3G_API void m3gBindImageTarget(M3GRenderContext hCtx, M3GImage hImage);
M3G_API M3Guint m3gGetUserHandle(M3GRenderContext hCtx);
M3G_API void m3gSetUserData(M3GRenderContext hCtx, M3Guint hData);
M3G_API M3Guint m3gGetUserData(M3GRenderContext hCtx);
M3G_API void m3gClear(M3GRenderContext context, M3GBackground hBackground);
M3G_API void m3gReleaseTarget(M3GRenderContext context);
M3G_API void m3gSetCamera(M3GRenderContext context, M3GCamera hCamera, M3GMatrix *transform);
M3G_API M3Gint m3gAddLight(M3GRenderContext hCtx, M3GLight hLight, const M3GMatrix *transform);
M3G_API void m3gSetLight(M3GRenderContext context, M3Gint lightIndex, M3GLight hLight, const M3GMatrix *transform);
M3G_API void m3gClearLights(M3GRenderContext context);
M3G_API void m3gSetViewport(M3GRenderContext hCtx, M3Gint x, M3Gint y, M3Gint width, M3Gint height);
M3G_API void m3gGetViewport(M3GRenderContext hCtx, M3Gint *x, M3Gint *y, M3Gint *width, M3Gint *height);
M3G_API void m3gSetClipRect(M3GRenderContext hCtx, M3Gint x, M3Gint y, M3Gint width, M3Gint height);
M3G_API void m3gSetDisplayArea(M3GRenderContext hCtx, M3Gint width, M3Gint height);
M3G_API void m3gSetDepthRange(M3GRenderContext hCtx, M3Gfloat depthNear, M3Gfloat depthFar);
M3G_API void m3gGetDepthRange(M3GRenderContext hCtx, M3Gfloat *depthNear, M3Gfloat *depthFar);
M3G_API void m3gGetViewTransform(M3GRenderContext hCtx, M3GMatrix *transform);
M3G_API M3GCamera m3gGetCamera(M3GRenderContext hCtx);
M3G_API M3GLight m3gGetLightTransform(M3GRenderContext hCtx, M3Gint lightIndex, M3GMatrix *transform);
M3G_API M3Gsizei m3gGetLightCount(M3GRenderContext hCtx);
M3G_API void m3gRenderWorld(M3GRenderContext context, M3GWorld hWorld);
M3G_API void m3gRenderNode(M3GRenderContext context, M3GNode hNode, const M3GMatrix *transform);
M3G_API void m3gRender(M3GRenderContext context, M3GVertexBuffer hVertices, M3GIndexBuffer hIndices, M3GAppearance hAppearance, const M3GMatrix *transformMatrix, M3Gfloat alphaFactor, M3Gint scope);
M3G_API void m3gBindEGLSurfaceTarget(M3GRenderContext context, M3GEGLSurface surface);
M3G_API void m3gBindWindowTarget(M3GRenderContext hCtx, M3GNativeWindow hWindow);
M3G_API void m3gInvalidateBitmapTarget(M3GRenderContext hCtx, M3GNativeBitmap hBitmap);
M3G_API void m3gInvalidateWindowTarget(M3GRenderContext hCtx, M3GNativeWindow hWindow);
M3G_API void m3gInvalidateMemoryTarget(M3GRenderContext hCtx, void *pixels);
M3G_API void m3gBindBitmapTarget(M3GRenderContext hCtx, M3GNativeBitmap hBitmap);
M3G_API void m3gBindMemoryTarget(M3GRenderContext context, void *pixels, M3Guint width, M3Guint height, M3GPixelFormat format, M3Guint stride, M3Guint userHandle);

/* AnimationController (m3g_animationcontroller.c) */

M3G_API M3GAnimationController m3gCreateAnimationController( M3GInterface hInterface);
M3G_API void m3gSetActiveInterval(M3GAnimationController hController, M3Gint worldTimeMin, M3Gint worldTimeMax);
M3G_API M3Gint m3gGetActiveIntervalStart(M3GAnimationController hController);
M3G_API M3Gint m3gGetActiveIntervalEnd(M3GAnimationController hController);
M3G_API void m3gSetSpeed(M3GAnimationController hController, M3Gfloat factor, M3Gint worldTime);
M3G_API M3Gfloat m3gGetSpeed(M3GAnimationController hController);
M3G_API void m3gSetPosition(M3GAnimationController hController, M3Gfloat sequenceTime, M3Gint worldTime);
M3G_API M3Gfloat m3gGetPosition(M3GAnimationController hController, M3Gint worldTime);
M3G_API void m3gSetWeight(M3GAnimationController hController, M3Gfloat weight);
M3G_API M3Gfloat m3gGetWeight(M3GAnimationController hController);
M3G_API M3Gint m3gGetRefWorldTime(M3GAnimationController hController);

/* AnimationTrack (m3g_animationtrack.c) */

M3G_API M3GAnimationTrack m3gCreateAnimationTrack(M3GInterface hInterface, M3GKeyframeSequence hSequence, M3Gint property);
M3G_API M3GAnimationController m3gGetController(M3GAnimationTrack hTrack);
M3G_API M3GKeyframeSequence m3gGetSequence(M3GAnimationTrack hTrack);
M3G_API M3Gint m3gGetTargetProperty(M3GAnimationTrack hTrack);
M3G_API void m3gSetController(M3GAnimationTrack hTrack, M3GAnimationController hController);

/* KeyframeSequence (m3g_keyframesequence.c) */

M3G_API M3GKeyframeSequence m3gCreateKeyframeSequence(M3GInterface hInterface, M3Gint numKeyframes, M3Gint numComponents, M3Gint interpolation);
M3G_API void m3gSetKeyframe(M3GKeyframeSequence handle, M3Gint ind, M3Gint time, M3Gint valueSize, const M3Gfloat *value);
M3G_API void m3gSetValidRange(M3GKeyframeSequence handle, M3Gint first, M3Gint last);
M3G_API void m3gSetDuration(M3GKeyframeSequence handle, M3Gint duration);
M3G_API M3Gint m3gGetDuration(M3GKeyframeSequence handle);
M3G_API M3Gint m3gGetComponentCount(M3GKeyframeSequence handle);
M3G_API M3Gint m3gGetInterpolationType(M3GKeyframeSequence handle);
M3G_API M3Gint m3gGetKeyframe(M3GKeyframeSequence handle, M3Gint frameIndex, M3Gfloat *value);
M3G_API M3Gint m3gGetKeyframeCount(M3GKeyframeSequence handle);
M3G_API void m3gGetValidRange(M3GKeyframeSequence handle, M3Gint *first, M3Gint *last);
M3G_API void m3gSetRepeatMode(M3GKeyframeSequence handle, M3Genum mode);
M3G_API M3Genum m3gGetRepeatMode(M3GKeyframeSequence handle);

/* Loader (m3g_loader.c) */

M3G_API M3GLoader m3gCreateLoader(M3GInterface m3g);
M3G_API void m3gImportObjects(M3GLoader loader, M3Gint n, M3GObject *refs);
M3G_API M3Gint m3gGetLoadedObjects(M3GLoader loader, M3GObject *buffer);
M3G_API M3Gsizei m3gDecodeData(M3GLoader loader, M3Gsizei bytes, const M3Gubyte *data);
M3G_API M3Gint m3gGetObjectsWithUserParameters(M3GLoader loader, M3GObject *objects);
M3G_API M3Gint m3gGetNumUserParameters(M3GLoader loader, M3Gint object);
M3G_API void m3gSetConstraints(M3GLoader loader, M3Gint triConstraint);
M3G_API M3Gsizei m3gGetUserParameter(M3GLoader loader, M3Gint object, M3Gint index, M3Gbyte *buffer);

/* Math utilities (m3g_math.c) */

M3G_API void m3gLerp(M3Gint size, M3Gfloat *vec, M3Gfloat s, const M3Gfloat *start, const M3Gfloat *end);
M3G_API void m3gHermite(M3Gint size, M3Gfloat *vec, M3Gfloat s, const M3Gfloat *start, const M3Gfloat *end, const M3Gfloat *tStart, const M3Gfloat *tEnd);
M3G_API void m3gCopyMatrix(M3GMatrix *dst, const M3GMatrix *src);
M3G_API void m3gAddVec3(M3GVec3 *vec, const M3GVec3 *other);
M3G_API void m3gAddVec4(M3GVec4 *vec, const M3GVec4 *other);
M3G_API void m3gCross(M3GVec3 *dst, const M3GVec3 *a, const M3GVec3 *b);
M3G_API M3Gfloat m3gDot3(const M3GVec3 *a, const M3GVec3 *b);
M3G_API M3Gfloat m3gDot4(const M3GVec4 *a, const M3GVec4 *b);
M3G_API void m3gSetVec3(M3GVec3 *v, M3Gfloat x, M3Gfloat y, M3Gfloat z);
M3G_API void m3gSetVec4(M3GVec4 *v, M3Gfloat x, M3Gfloat y, M3Gfloat z, M3Gfloat w);
M3G_API void m3gSubVec3(M3GVec3 *vec, const M3GVec3 *other);
M3G_API void m3gSubVec4(M3GVec4 *vec, const M3GVec4 *other);
M3G_API M3Gfloat m3gLengthVec3(const M3GVec3 *vec);
M3G_API void m3gScaleVec3(M3GVec3 *vec, const M3Gfloat s);
M3G_API void m3gScaleVec4(M3GVec4 *vec, const M3Gfloat s);
M3G_API void m3gGetAngleAxis(const M3GQuat *quat, M3Gfloat *angle, M3GVec3 *axis);
M3G_API void m3gGetMatrixColumn(const M3GMatrix *mtx, M3Gint col, M3GVec4 *dst);
M3G_API void m3gGetMatrixColumns(const M3GMatrix *mtx, M3Gfloat *dst);
M3G_API void m3gGetMatrixRow(const M3GMatrix *mtx, M3Gint row, M3GVec4 *dst);
M3G_API void m3gGetMatrixRows(const M3GMatrix *mtx, M3Gfloat *dst);
M3G_API void m3gIdentityMatrix(M3GMatrix *mtx);
M3G_API void m3gIdentityQuat(M3GQuat *quat);
M3G_API M3Gbool m3gInvertMatrix(M3GMatrix *mtx);
M3G_API M3Gbool m3gMatrixInverse(M3GMatrix *mtx, const M3GMatrix *other);
M3G_API void m3gMatrixTranspose(M3GMatrix *mtx, const M3GMatrix *other);
M3G_API M3Gbool m3gInverseTranspose(M3GMatrix *mtx, const M3GMatrix *other);
M3G_API void m3gMatrixProduct(M3GMatrix *dst, const M3GMatrix *left, const M3GMatrix *right);
M3G_API void m3gNormalizeQuat(M3GQuat *q);
M3G_API void m3gNormalizeVec3(M3GVec3 *v);
M3G_API void m3gNormalizeVec4(M3GVec4 *v);
M3G_API void m3gPostMultiplyMatrix(M3GMatrix *mtx, const M3GMatrix *other);
M3G_API void m3gPreMultiplyMatrix(M3GMatrix *mtx, const M3GMatrix *other);
M3G_API void m3gPostRotateMatrix(M3GMatrix *mtx, M3Gfloat angle, M3Gfloat ax, M3Gfloat ay, M3Gfloat az);
M3G_API void m3gPostRotateMatrixQuat(M3GMatrix *mtx, const M3GQuat *quat);
M3G_API void m3gPostScaleMatrix(M3GMatrix *mtx, M3Gfloat sx, M3Gfloat sy, M3Gfloat sz);
M3G_API void m3gPostTranslateMatrix(M3GMatrix *mtx, M3Gfloat tx, M3Gfloat ty, M3Gfloat tz);
M3G_API void m3gPreRotateMatrix(M3GMatrix *mtx, M3Gfloat angle, M3Gfloat ax, M3Gfloat ay, M3Gfloat az);
M3G_API void m3gPreRotateMatrixQuat(M3GMatrix *mtx, const M3GQuat *quat);
M3G_API void m3gPreScaleMatrix(M3GMatrix *mtx, M3Gfloat sx, M3Gfloat sy, M3Gfloat sz);
M3G_API void m3gPreTranslateMatrix(M3GMatrix *mtx, M3Gfloat tx, M3Gfloat ty, M3Gfloat tz);
M3G_API void m3gQuatMatrix(M3GMatrix *mtx, const M3GQuat *quat);
M3G_API void m3gScalingMatrix( M3GMatrix *mtx, const M3Gfloat sx, const M3Gfloat sy, const M3Gfloat sz);
M3G_API void m3gSetAngleAxis(M3GQuat *quat, M3Gfloat angle, M3Gfloat ax, M3Gfloat ay, M3Gfloat az);
M3G_API void m3gSetAngleAxisRad(M3GQuat *quat, M3Gfloat angleRad, M3Gfloat ax, M3Gfloat ay, M3Gfloat az);
M3G_API void m3gMulQuat(M3GQuat *quat, const M3GQuat *other);
M3G_API void m3gSetQuatRotation(M3GQuat *quat, const M3GVec3 *from, const M3GVec3 *to);
M3G_API void m3gSetMatrixColumns(M3GMatrix *mtx, const M3Gfloat *src);
M3G_API void m3gSetMatrixRows(M3GMatrix *mtx, const M3Gfloat *src);
M3G_API void m3gTransformVec4(const M3GMatrix *mtx, M3GVec4 *vec);
M3G_API void m3gTranslationMatrix( M3GMatrix *mtx, const M3Gfloat tx, const M3Gfloat ty, const M3Gfloat tz);
M3G_API void m3gSetQuat(M3GQuat *quat, const M3Gfloat *vec);
M3G_API void m3gSlerpQuat(M3GQuat *quat, M3Gfloat s, const M3GQuat *q0, const M3GQuat *q1);
M3G_API void m3gSquadQuat(M3GQuat *quat, M3Gfloat s, const M3GQuat *q0, const M3GQuat *a, const M3GQuat *b, const M3GQuat *q1);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /*__M3G_CORE_H__*/
