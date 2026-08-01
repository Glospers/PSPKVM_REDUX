/*
 * m3g_load_test.c -- host-side .m3g parse check.
 *
 * Runs the exact call sequence the KNI natives run (m3gPspLoadFromMemory, see
 * ../src/m3g_psp_loader.c) against real files, on the build machine, so that
 * the engine bring-up can be verified without a PSP in the loop.  Feed it the
 * .m3g files out of a MIDlet JAR:
 *
 *   make -C m3g/test && m3g/test/m3g_load_test /tmp/jar/data/3d/*.m3g
 *
 * It prints one line per file with the number of root objects and the class of
 * each, and exits non-zero if any file failed to parse.  A scene file normally
 * has exactly one root, a World.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include "M3G/m3g_psp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Names of the M3GClass enumerators, indexed by value; M3G/m3g_core.h:272. */
static const char *const CLASS_NAMES[] = {
    "AbstractObject",
    "AnimationController", "AnimationTrack", "Appearance", "Background",
    "Camera", "CompositingMode", "Fog", "Group", "Image2D", "IndexBuffer",
    "KeyframeSequence", "Light", "Loader", "Material", "Mesh", "MorphingMesh",
    "PolygonMode", "RenderContext", "SkinnedMesh", "Sprite3D", "Texture2D",
    "VertexArray", "VertexBuffer", "World"
};

#define CLASS_NAME_COUNT ((int)(sizeof(CLASS_NAMES) / sizeof(CLASS_NAMES[0])))

static const char *className(M3Gint id)
{
    if (id < 0 || id >= CLASS_NAME_COUNT) {
        return "?";
    }
    return CLASS_NAMES[id];
}

static const char *errorName(M3Gint code)
{
    switch (code) {
    case M3G_PSP_ERR_INVALID:       return "invalid argument";
    case M3G_PSP_ERR_NO_INTERFACE:  return "interface creation failed";
    case M3G_PSP_ERR_OUT_OF_MEMORY: return "out of memory";
    case M3G_PSP_ERR_IO:            return "malformed or truncated file";
    case M3G_PSP_ERR_UNSUPPORTED:   return "unsupported format / no roots";
    default:                        return "unknown error";
    }
}

static M3Gubyte *readFile(const char *path, long *lengthOut)
{
    FILE *f = fopen(path, "rb");
    long length;
    M3Gubyte *data;

    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    length = ftell(f);
    rewind(f);

    if (length <= 0) {
        fclose(f);
        return NULL;
    }

    data = (M3Gubyte *) malloc((size_t) length);
    if (data == NULL) {
        fclose(f);
        return NULL;
    }
    if (fread(data, 1, (size_t) length, f) != (size_t) length) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);

    *lengthOut = length;
    return data;
}

/*!
 * \brief Prints the user parameters a file attached to its objects.
 *
 * JSR-184 requires Loader.load to hand these back as each object's user
 * object, so a title that stores metadata in them -- node roles, collision
 * flags, spawn points -- reads null instead of a Hashtable if this comes back
 * empty.  Printing the first bytes of each value makes it obvious whether a
 * parameter is text or packed binary.
 */
static void printUserParameters(const M3GPspLoadResult *result)
{
    M3Gint object;

    if (result->userCount <= 0) {
        printf("      user parameters: none\n");
        return;
    }

    printf("      user parameters: %d object(s)\n", (int) result->userCount);

    for (object = 0; object < result->userCount; ++object) {
        M3Gint params = m3gPspGetUserParamCount(result, object);
        M3Gint index;

        printf("        %s@%p: %d parameter(s)\n",
               className(m3gPspGetClassID(result->userObjects[object])),
               (void *) result->userObjects[object], (int) params);

        for (index = 0; index < params; ++index) {
            M3Gint len = m3gPspGetUserParamLength(result, object, index);
            M3Gubyte buffer[64];
            M3Gint id, i, shown;

            if (len < 0 || len > (M3Gint) sizeof(buffer)) {
                printf("          [%d] %d bytes (not shown)\n",
                       (int) index, (int) len);
                continue;
            }
            id = m3gPspGetUserParam(result, object, index, buffer);

            shown = len;
            printf("          id=%d len=%d  ", (int) id, (int) len);
            for (i = 0; i < shown; ++i) {
                printf("%02x", (unsigned) buffer[i]);
            }
            printf("  \"");
            for (i = 0; i < shown; ++i) {
                int c = buffer[i];
                putchar((c >= 32 && c < 127) ? c : '.');
            }
            printf("\"\n");
        }
    }
}

/*----------------------------------------------------------------------
 * The walk a MIDlet does after loading
 *
 * A title does not stop at the roots: it descends the graph and reads the
 * pieces back out, which on the Java side means Group.getChild,
 * Mesh.getAppearance, Appearance.getTexture and so on, each of which is an
 * engine accessor wrapped in an Object3D.  Where the engine answers with
 * nothing, the wrapper is null, and a MIDlet that assumed otherwise throws a
 * NullPointerException the moment it uses it.
 *
 * Doing the same walk here says which of those nulls a given file actually
 * produces, on the build machine, without a PSP or an emulator in the loop.
 *--------------------------------------------------------------------*/

static int g_nulls;

static void walkNode(M3GObject object, int depth, const char *role);

/*! \brief Reports an accessor that answered with nothing, and counts it. */
static void reportNull(int depth, const char *what, M3GObject owner)
{
    printf("      %*sNULL <- %s of %s@%p\n", depth * 2, "",
           what, className(m3gPspGetClassID(owner)), (void *) owner);
    g_nulls++;
}

static void walkAppearance(M3GObject appearance, int depth)
{
    M3GObject material = (M3GObject) m3gGetMaterial((M3GAppearance) appearance);
    M3Gint unit;

    if (material == NULL) {
        reportNull(depth, "getMaterial()", appearance);
    }
    /* The title sets and reads texture unit 0 and 1; M3G guarantees at least
     * two, and the engine reports the rest as absent rather than failing. */
    for (unit = 0; unit < 2; ++unit) {
        M3GObject texture =
            (M3GObject) m3gGetTexture((M3GAppearance) appearance, unit);
        if (texture == NULL) {
            continue;   /* an unused unit is normal, not a defect */
        }
        if ((M3GObject) m3gGetTextureImage((M3GTexture) texture) == NULL) {
            reportNull(depth, "Texture2D.getImage()", texture);
        }
    }
}

/*
 * Set with -t: re-texture every appearance the walk finds, the way a title
 * does after loading a scene.
 *
 * The Java layer passes the engine handle of the Texture2D it was given, and a
 * Texture2D a MIDlet built before the renderer came up has none -- so what
 * reaches the engine is a null texture.  That drops the reference the file put
 * on the texture the appearance already had, and with it the image underneath,
 * which is a good deal more than "assign a field".
 */
static int g_retexture;

static void walkMesh(M3GObject mesh, int depth)
{
    M3Gint submeshes = m3gGetSubmeshCount((M3GMesh) mesh);
    M3Gint i;

    if ((M3GObject) m3gGetVertexBuffer((M3GMesh) mesh) == NULL) {
        reportNull(depth, "getVertexBuffer()", mesh);
    }
    for (i = 0; i < submeshes; ++i) {
        M3GObject appearance = (M3GObject) m3gGetAppearance((M3GMesh) mesh, i);
        if (appearance == NULL) {
            reportNull(depth, "getAppearance(i)", mesh);
            continue;
        }
        walkAppearance(appearance, depth + 1);

        if (g_retexture) {
            /*
             * The exact sequence the title runs over every submesh it loads,
             * in order (bk.a(Node, Texture2D[], boolean) in the MIDlet). Every
             * object it hands over is one it built itself, and a MIDlet's own
             * objects have no engine object behind them until a render target
             * has been bound -- so each of these arrives as a null, and each
             * one drops the reference the file put on what was there before.
             */
            printf("      %*sre-dress %s@%p\n", depth * 2, "",
                   className(m3gPspGetClassID(appearance)), (void *) appearance);
            fflush(stdout);

            m3gSetCompositingMode((M3GAppearance) appearance, NULL);
            m3gSetPolygonMode((M3GAppearance) appearance, NULL);
            m3gSetMaterial((M3GAppearance) appearance, NULL);
            if ((M3GObject) m3gGetTexture((M3GAppearance) appearance, 0) != NULL) {
                m3gSetTexture((M3GAppearance) appearance, 0, NULL);
            }
            m3gSetAppearance((M3GMesh) mesh, i, (M3GAppearance) appearance);
        }
    }
}

static void walkNode(M3GObject object, int depth, const char *role)
{
    M3Gint classID = m3gPspGetClassID(object);
    M3Gint tracks, i;

    if (object == NULL) {
        return;
    }
    if (depth > 16) {
        printf("      (depth limit reached, cycle?)\n");
        return;
    }

    /* Animation is read the same way, and a track without a sequence is the
     * other null a title can walk into. */
    tracks = m3gGetAnimationTrackCount(object);
    for (i = 0; i < tracks; ++i) {
        M3GObject track = (M3GObject) m3gGetAnimationTrack(object, i);
        if (track == NULL) {
            reportNull(depth, "getAnimationTrack(i)", object);
        }
        else if ((M3GObject) m3gGetSequence((M3GAnimationTrack) track) == NULL) {
            reportNull(depth, "getKeyframeSequence()", track);
        }
    }

    switch (classID) {
    case 8:                                     /* Group, and World with it  */
    case 24:
        {
            M3Gint children = m3gGetChildCount((M3GGroup) object);
            for (i = 0; i < children; ++i) {
                M3GObject child = (M3GObject) m3gGetChild((M3GGroup) object, i);
                if (child == NULL) {
                    reportNull(depth, "getChild(i)", object);
                    continue;
                }
                walkNode(child, depth + 1, "child");
            }
        }
        break;

    case 15:                                    /* Mesh                      */
    case 16:                                    /* MorphingMesh              */
    case 19:                                    /* SkinnedMesh               */
        walkMesh(object, depth);
        break;

    default:
        break;
    }
    (void) role;
}

static int loadOne(const char *path)
{
    long length = 0;
    M3Gubyte *data = readFile(path, &length);
    M3GPspLoadResult result;
    M3Gint count, i;

    if (data == NULL) {
        printf("%-40s  CANNOT READ\n", path);
        return 1;
    }

    count = m3gPspLoadFromMemory(data, (M3Gsizei) length, &result);
    free(data);

    if (count < 0) {
        printf("%-40s  %6ld bytes  FAILED (%s)\n",
               path, length, errorName(count));
        return 1;
    }

    printf("%-40s  %6ld bytes  %d root%s:",
           path, length, (int) count, (count == 1) ? "" : "s");
    for (i = 0; i < count; ++i) {
        printf(" %s@%p", className(m3gPspGetClassID(result.roots[i])),
               (void *) result.roots[i]);
    }
    printf("\n");

    printUserParameters(&result);

    {
        int before = g_nulls;
        for (i = 0; i < count; ++i) {
            walkNode(result.roots[i], 3, "root");
        }
        printf("      walk: %d accessor(s) answered with nothing\n",
               g_nulls - before);
    }

    /*
     * With -t the scene is kept, because the title keeps it: it loads the
     * pieces of a station one after another and holds all of them at once, so
     * every load after the first runs against an arena the previous ones are
     * still occupying.  Releasing here instead would hand each file a nearly
     * empty arena and test something the title never does.
     */
    if (g_retexture) {
        m3gPspFinishResult(&result);
    }
    else {
        /* Hand the references back, which also exercises the teardown path
         * the Java abort case uses. */
        m3gPspReleaseResult(&result);
    }
    return 0;
}

int main(int argc, char **argv)
{
    int failures = 0;
    int i;

    if (argc >= 2 && strcmp(argv[1], "-t") == 0) {
        g_retexture = 1;
        argv++;
        argc--;
    }

    if (argc < 2) {
        fprintf(stderr, "usage: %s [-t] <file.m3g> [...]\n", argv[0]);
        return 2;
    }

    if (m3gPspGetInterface() == NULL) {
        fprintf(stderr, "m3gCreateInterface failed\n");
        return 2;
    }

    for (i = 1; i < argc; ++i) {
        failures += loadOne(argv[i]);
    }

    printf("\n%d file(s), %d failure(s)\n", argc - 1, failures);

    /*
     * The engine allocates from its own guarded heap rather than the C heap
     * (../src/m3g_psp_arena.c), which makes this harness a memory-safety test
     * as well as a parse test: every block carries a canary, the arena has a
     * guard band at each end, and m3gPspLoadFromMemory sweeps the lot after
     * every file.  A non-zero corrupt= count is an out-of-bounds write by the
     * engine and fails the run regardless of whether anything parsed.
     */
    {
        M3GPspArenaStats st;
        M3Gint fault;

        fault = m3gPspArenaVerify();
        m3gPspArenaGetStats(&st);

        printf("arena: cap=%d used=%d peak=%d blocks=%d "
               "alloc-failures=%d corrupt=%d fault=%d at=0x%08x\n",
               (int) st.capacity, (int) st.used, (int) st.peak,
               (int) st.blocks, (int) st.failures, (int) st.corrupt,
               (int) st.fault, (unsigned) st.firstBad);

        if (st.corrupt != 0 || fault != M3G_PSP_ARENA_OK) {
            printf("ARENA CORRUPTION DETECTED\n");
            failures++;
        }
        if (st.failures != 0) {
            printf("ARENA TOO SMALL -- raise M3G_PSP_ARENA_KB\n");
            failures++;
        }
    }

    return (failures == 0) ? 0 : 1;
}
