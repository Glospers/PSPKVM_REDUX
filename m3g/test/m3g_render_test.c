/*
 * m3g_render_test.c -- does the engine emit draw calls for a real scene?
 *
 * The title's 3D area renders flat: every renderNode reports success and not
 * one triangle reaches a pixel.  The engine culls scene nodes on the CPU
 * before touching GL, and its whole transform stack runs through the five
 * reconstructed functions in ../src/m3g_math_compat.c -- so "transformed or
 * culled away inside the engine" is a hypothesis this harness can test on the
 * build machine, against the stub GL backend, by counting the draw calls that
 * come out the far side.
 *
 * It repeats the title's exact call sequence (Loader roots, then
 * setCamera / clear / renderNode -- never render(World)) for one scene at a
 * range of camera distances.  Zero draws everywhere says the scene dies
 * inside the engine, and the fault is host-debuggable; healthy draw counts
 * acquit the engine and move the search into the GL backend.
 *
 *   make -C m3g/test m3g_render_test M3GCORE_DIR=... && \
 *       m3g_render_test scenes/station_starter.m3g
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include "M3G/m3g_psp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* stub/m3g_gl_stub.c */
extern int m3gStubDrawCalls;
extern int m3gStubDrawVertices;

static M3Gubyte *readFile(const char *path, long *lengthOut)
{
    FILE *f = fopen(path, "rb");
    long length;
    M3Gubyte *data;

    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    rewind(f);
    if (length <= 0) {
        fclose(f);
        return NULL;
    }
    data = (M3Gubyte *) malloc((size_t) length);
    if (data == NULL || fread(data, 1, (size_t) length, f) != (size_t) length) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *lengthOut = length;
    return data;
}

int main(int argc, char **argv)
{
    static unsigned short target[480 * 272];
    static const float distances[] = { 0.f, 5.f, 20.f, 50.f, 200.f, 1000.f };

    M3GPspLoadResult result;
    M3GInterface m3g;
    M3GCamera camera;
    long length = 0;
    M3Gubyte *data;
    M3Gint count;
    int d, i;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.m3g>\n", argv[0]);
        return 2;
    }

    m3g = m3gPspGetInterface();
    if (m3g == NULL) {
        fprintf(stderr, "no interface\n");
        return 2;
    }

    data = readFile(argv[1], &length);
    if (data == NULL) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }

    count = m3gPspLoadFromMemory(data, (M3Gsizei) length, &result);
    free(data);
    if (count <= 0) {
        fprintf(stderr, "load failed: %d\n", (int) count);
        return 1;
    }
    printf("%s: %d root(s)\n", argv[1], (int) count);

    /* The title's frame, piece for piece: bind, camera, clear, render. */

    if (m3gPspBindMemoryTarget(target, 480, 272, 480 * 2, 1, 0)
            != M3G_PSP_RENDER_OK) {
        fprintf(stderr, "bind failed\n");
        return 1;
    }

    camera = m3gCreateCamera(m3g);
    m3gSetPerspective(camera, 60.0f, 480.0f / 272.0f, 1.0f, 10000.0f);

    for (d = 0; d < (int) (sizeof(distances) / sizeof(distances[0])); ++d) {
        /* Row-major, as Transform.get() would produce: the identity with the
         * scene pushed distances[d] down -Z, i.e. in front of the camera. */
        float transform[16] = {
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };
        transform[11] = -distances[d];

        m3gPspSetCamera((M3GObject) camera, NULL);
        m3gPspClear(NULL);

        m3gStubDrawCalls = 0;
        m3gStubDrawVertices = 0;

        for (i = 0; i < count; ++i) {
            M3Gint err = m3gPspRenderNode(result.roots[i], transform);
            if (err != M3G_PSP_RENDER_OK) {
                printf("  z=%-7.1f root %d: renderNode error %d\n",
                       distances[d], i, (int) err);
            }
        }
        printf("  z=%-7.1f draws=%-4d vertices=%d\n",
               distances[d], m3gStubDrawCalls, m3gStubDrawVertices);
    }

    m3gPspReleaseTarget();
    m3gPspReleaseResult(&result);
    return 0;
}
