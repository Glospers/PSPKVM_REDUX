/*
 * m3g_psp_loader.c -- interface bootstrap and .m3g parsing for the PSP port.
 *
 * Everything here is m3gcore bookkeeping that has exactly one correct answer,
 * written once so the KNI natives and the host-side test harness cannot drift
 * apart.  See inc/M3G/m3g_psp.h for the contract.
 *
 * Two details are worth spelling out because getting either wrong is silent:
 *
 * 1. m3gDecodeData() installs a NULL error handler for the duration of the
 *    parse (src/m3g_loader.c:2925) and re-raises through the real one at the
 *    end, so an installed callback fires exactly once per failed load.  We do
 *    not install one at all and read m3gGetError() instead, which returns the
 *    stored code and clears it (src/m3g_interface.c:1782).  Same information,
 *    no callback re-entrancy into KNI.
 *
 * 2. The loader holds one reference on every object it creates
 *    (src/m3g_loader.c:2664) and drops all of them when it is destroyed
 *    (m3gCleanupLoader, :2723).  Root objects -- the ones nothing else in the
 *    file points at, which is what m3gGetLoadedObjects returns -- would
 *    therefore hit refcount 0 and be freed along with the loader.  So we take
 *    our own reference on each root *before* destroying it.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include "M3G/m3g_psp.h"

#include <stddef.h>     /* NULL only -- nothing here allocates from libc any
                         * more; see the note on mallocFunc below */

/*----------------------------------------------------------------------
 * The interface singleton
 *--------------------------------------------------------------------*/

static M3GInterface s_interface = NULL;

M3GInterface m3gPspGetInterface(void)
{
    if (s_interface == NULL) {
        M3Gparams params;

        /* Zero first: M3Gparams has nine members and only two of them are
         * mandatory (src/m3g_interface.c:1597-1604); the rest must be NULL,
         * not garbage, or m3gCreateInterface will happily install a stack
         * address as the error callback.
         *
         * mallocFunc/freeFunc deliberately do NOT go to libc.  See the note
         * above the arena entry points in inc/M3G/m3g_psp.h: on this platform
         * the C heap contains the Java object heap, so an engine block
         * allocated from it sits against VM memory.  src/m3g_psp_arena.c is a
         * separate static heap with guard bands, which both prevents that and
         * detects it if it happens anyway. */
        params.mallocFunc      = m3gPspArenaAlloc;
        params.freeFunc        = m3gPspArenaFree;
        params.objAllocFunc    = NULL;
        params.objResolveFunc  = NULL;
        params.objFreeFunc     = NULL;
        params.errorFunc       = NULL;
        params.beginRenderFunc = NULL;
        params.endRenderFunc   = NULL;
        params.userContext     = NULL;

        s_interface = m3gCreateInterface(&params);
    }
    return s_interface;
}

/*----------------------------------------------------------------------
 * Loading
 *--------------------------------------------------------------------*/

static M3Gint m3gPspMapError(M3Genum error)
{
    switch (error) {
    case M3G_OUT_OF_MEMORY: return M3G_PSP_ERR_OUT_OF_MEMORY;
    case M3G_INVALID_VALUE: return M3G_PSP_ERR_INVALID;
    default:                return M3G_PSP_ERR_IO;
    }
}

M3Gint m3gPspLoadFromMemory(const M3Gubyte *data,
                            M3Gsizei length,
                            M3GObject **objects)
{
    M3GInterface m3g;
    M3GLoader loader;
    M3GObject *roots;
    M3Gsizei bytesLeft;
    M3Genum error;
    M3Gint count, i;

    if (objects != NULL) {
        *objects = NULL;
    }
    if (data == NULL || length <= 0 || objects == NULL) {
        return M3G_PSP_ERR_INVALID;
    }

    m3g = m3gPspGetInterface();
    if (m3g == NULL) {
        return M3G_PSP_ERR_NO_INTERFACE;
    }

    /* Discard anything a previous operation left behind, so the code below
     * can attribute what it reads to this load. */
    m3gGetError(m3g);

    loader = m3gCreateLoader(m3g);
    if (loader == NULL) {
        m3gGetError(m3g);
        return M3G_PSP_ERR_OUT_OF_MEMORY;
    }

    /* The loader buffers internally and loops until it runs out of input, so
     * the whole file can go in as one block; the return value is how much
     * more it still wants, and zero means the file is complete. */
    bytesLeft = m3gDecodeData(loader, length, data);

    error = m3gGetError(m3g);
    if (error != M3G_NO_ERROR) {
        m3gDeleteObject((M3GObject) loader);
        return m3gPspMapError(error);
    }
    if (bytesLeft != 0) {
        /* Well-formed so far but the file ends mid-object. */
        m3gDeleteObject((M3GObject) loader);
        return M3G_PSP_ERR_IO;
    }

    count = m3gGetLoadedObjects(loader, NULL);
    if (count <= 0) {
        /* Also the PNG path: the identifier check parks the loader in
         * LOADSTATE_NOT_SUPPORTED without raising an error
         * (src/m3g_loader.c:500), and m3gGetLoadedObjects reports zero for
         * any state below LOADSTATE_INITIAL. */
        m3gDeleteObject((M3GObject) loader);
        return M3G_PSP_ERR_UNSUPPORTED;
    }

    /* Out of the arena as well, not libc: this array is written by
     * m3gGetLoadedObjects below and read by the KNI natives, so if the count
     * and the fill ever disagreed the overrun would land in the Java heap.
     * Inside the arena the block canary catches it instead. */
    roots = (M3GObject *) m3gPspArenaAlloc(
                (M3Guint) count * (M3Guint) sizeof(M3GObject));
    if (roots == NULL) {
        m3gDeleteObject((M3GObject) loader);
        return M3G_PSP_ERR_OUT_OF_MEMORY;
    }
    m3gGetLoadedObjects(loader, roots);

    for (i = 0; i < count; ++i) {
        m3gAddRef(roots[i]);
    }

    m3gDeleteObject((M3GObject) loader);

    /* Sweep the arena while we still know which load was the last one to
     * touch it.  The result is sticky (m3gPspArenaGetStats reports the first
     * violation and its address), so callers only have to read the counters;
     * doing it here means neither the KNI natives nor the test harness can
     * forget to. */
    m3gPspArenaVerify();

    *objects = roots;
    return count;
}

void m3gPspReleaseRoots(M3GObject *objects, M3Gint count)
{
    M3Gint i;

    if (objects == NULL) {
        return;
    }
    for (i = 0; i < count; ++i) {
        if (objects[i] != NULL) {
            m3gDeleteRef(objects[i]);
        }
    }
    m3gPspArenaFree(objects);
}

void m3gPspFreeRootArray(M3GObject *objects)
{
    m3gPspArenaFree(objects);
}

M3Gint m3gPspGetClassID(M3GObject object)
{
    if (object == NULL) {
        return -1;
    }
    return (M3Gint) m3gGetClass(object);
}
