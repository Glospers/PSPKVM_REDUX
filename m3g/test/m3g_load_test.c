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

static int loadOne(const char *path)
{
    long length = 0;
    M3Gubyte *data = readFile(path, &length);
    M3GObject *roots = NULL;
    M3Gint count, i;

    if (data == NULL) {
        printf("%-40s  CANNOT READ\n", path);
        return 1;
    }

    count = m3gPspLoadFromMemory(data, (M3Gsizei) length, &roots);
    free(data);

    if (count < 0) {
        printf("%-40s  %6ld bytes  FAILED (%s)\n",
               path, length, errorName(count));
        return 1;
    }

    printf("%-40s  %6ld bytes  %d root%s:",
           path, length, (int) count, (count == 1) ? "" : "s");
    for (i = 0; i < count; ++i) {
        printf(" %s@%p", className(m3gPspGetClassID(roots[i])),
               (void *) roots[i]);
    }
    printf("\n");

    /* Nothing here keeps the scene, so hand the references back -- which also
     * exercises the teardown path the Java abort case uses. */
    m3gPspReleaseRoots(roots, count);
    return 0;
}

int main(int argc, char **argv)
{
    int failures = 0;
    int i;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.m3g> [...]\n", argv[0]);
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
