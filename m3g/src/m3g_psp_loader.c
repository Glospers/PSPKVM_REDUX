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
 * 3. The loader is destroyed by the caller, not here.  A file's user
 *    parameters live in the loader (:2980-3050) and are freed with it, so a
 *    load that tore it down on the way out could never hand them back.  See
 *    M3GPspLoadResult in inc/M3G/m3g_psp.h.
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

/*!
 * \brief The interface if it exists, never creating one.
 *
 * See the note in inc/M3G/m3g_psp.h: creating the interface starts pspgl, and
 * doing that while PSPKVM is drawing its own 2D wedges the display. Object
 * construction therefore asks with this and does nothing if the answer is
 * NULL; only binding a target and loading a file may bring the renderer up.
 */
M3GInterface m3gPspPeekInterface(void)
{
    return s_interface;
}

M3GInterface m3gPspGetInterface(void)
{
    if (s_interface == NULL) {
        M3Gparams params;

        /* Before anything else: m3gCreateInterface brings GL up to read the
         * driver's limits (src/m3g_interface.c:1679 -> m3gConfigureGL), and
         * that creates a pbuffer -- pspgl's first video memory allocation.
         * Its allocator starts at edram + 0, which is where PSPKVM's frame
         * buffers already are, so the region has to be spoken for first.
         * See src/m3g_psp_vidmem.c. */
        m3gPspReserveVram();

        /* Then bring EGL up and keep a context current, before the engine can
         * touch GL at all. m3gcore only makes one current inside a bound
         * target, but reaches GL well outside one -- committing an Image2D
         * uploads its texture immediately (m3gcore/src/m3g_image.inl:146) --
         * and pspgl writes its command stream through a global current-context
         * pointer it never checks. Doing this first also means m3gCreateInterface
         * finds EGL already up and keeps it that way (m3g_interface.c:1382)
         * instead of terminating it when its probe finishes. */
        m3gPspHoldGLContext();

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

/*! \brief Empties a result without touching anything it used to own. */
static void m3gPspClearResult(M3GPspLoadResult *result)
{
    result->loader      = NULL;
    result->roots       = NULL;
    result->rootCount   = 0;
    result->userObjects = NULL;
    result->userCount   = 0;
}

M3Gint m3gPspLoadFromMemory(const M3Gubyte *data,
                            M3Gsizei length,
                            M3GPspLoadResult *result)
{
    M3GInterface m3g;
    M3GLoader loader;
    M3GObject *roots;
    M3Gsizei bytesLeft;
    M3Genum error;
    M3Gint count, i;

    if (result != NULL) {
        m3gPspClearResult(result);
    }
    if (data == NULL || length <= 0 || result == NULL) {
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

    result->loader    = loader;
    result->roots     = roots;
    result->rootCount = count;

    /* The objects that came out of the file with user parameters attached.
     * They are named here rather than on demand because the engine only
     * offers them all at once, and the caller reads them one at a time.
     *
     * A file without any is the normal case and not a failure; so is an arena
     * too full to hold the array, which costs the parameters and nothing
     * else -- the scene itself is already built and usable. */
    result->userCount = m3gGetObjectsWithUserParameters(loader, NULL);
    if (result->userCount > 0) {
        result->userObjects = (M3GObject *) m3gPspArenaAlloc(
            (M3Guint) result->userCount * (M3Guint) sizeof(M3GObject));
        if (result->userObjects != NULL) {
            m3gGetObjectsWithUserParameters(loader, result->userObjects);
        }
        else {
            result->userCount = 0;
        }
    }
    else {
        result->userCount = 0;
    }

    /* Sweep the arena while we still know which load was the last one to
     * touch it.  The result is sticky (m3gPspArenaGetStats reports the first
     * violation and its address), so callers only have to read the counters;
     * doing it here means neither the KNI natives nor the test harness can
     * forget to. */
    m3gPspArenaVerify();

    return count;
}

/*!
 * \brief The half of releasing a result that is the same either way.
 *
 * Destroying the loader is what frees the user-parameter table, so the arrays
 * that index into it go at the same moment.
 */
static void m3gPspEndResult(M3GPspLoadResult *result)
{
    if (result->userObjects != NULL) {
        m3gPspArenaFree(result->userObjects);
    }
    if (result->roots != NULL) {
        m3gPspArenaFree(result->roots);
    }
    if (result->loader != NULL) {
        m3gDeleteObject((M3GObject) result->loader);
    }
    m3gPspClearResult(result);
}

void m3gPspReleaseResult(M3GPspLoadResult *result)
{
    M3Gint i;

    if (result == NULL) {
        return;
    }
    if (result->roots != NULL) {
        for (i = 0; i < result->rootCount; ++i) {
            if (result->roots[i] != NULL) {
                m3gDeleteRef(result->roots[i]);
            }
        }
    }
    m3gPspEndResult(result);
}

void m3gPspFinishResult(M3GPspLoadResult *result)
{
    if (result == NULL) {
        return;
    }
    m3gPspEndResult(result);
}

/*----------------------------------------------------------------------
 * User parameters
 *
 * Thin wrappers, but they are the whole reason the loader is kept alive, and
 * putting them here keeps the two conventions m3gGetUserParameter carries --
 * a NULL buffer asks for the length, a real one copies and answers with the
 * id -- from having to be remembered anywhere else.
 *--------------------------------------------------------------------*/

M3Gint m3gPspGetUserParamCount(const M3GPspLoadResult *result, M3Gint object)
{
    if (result == NULL || result->loader == NULL
        || object < 0 || object >= result->userCount) {
        return 0;
    }
    return m3gGetNumUserParameters(result->loader, object);
}

M3Gint m3gPspGetUserParamLength(const M3GPspLoadResult *result,
                                M3Gint object, M3Gint index)
{
    if (result == NULL || result->loader == NULL
        || object < 0 || object >= result->userCount) {
        return 0;
    }
    return (M3Gint) m3gGetUserParameter(result->loader, object, index, NULL);
}

M3Gint m3gPspGetUserParam(const M3GPspLoadResult *result,
                          M3Gint object, M3Gint index, void *buffer)
{
    if (result == NULL || result->loader == NULL || buffer == NULL
        || object < 0 || object >= result->userCount) {
        return 0;
    }
    return (M3Gint) m3gGetUserParameter(result->loader, object, index,
                                        (M3Gbyte *) buffer);
}

M3Gint m3gPspGetClassID(M3GObject object)
{
    if (object == NULL) {
        return -1;
    }
    return (M3Gint) m3gGetClass(object);
}
