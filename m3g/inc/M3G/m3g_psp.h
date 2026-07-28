/*
 * m3g_psp.h -- the platform-facing entry points of the M3G port.
 *
 * m3gcore is handle based and stateless about who owns what: an application
 * creates an M3GInterface (the heap + callback bundle every object hangs off),
 * then drives an M3GLoader by hand -- feed bytes, check for a raised error,
 * collect the unreferenced objects, take a reference on each of them, destroy
 * the loader.  Getting that dance wrong leaks the whole scene or frees it out
 * from under the caller, so it is written exactly once, here, and both users
 * call it:
 *
 *   - jsr184/src/native/m3g_loader_kni.c, the KNI natives behind
 *     javax.microedition.m3g.Loader; and
 *   - test/m3g_load_test.c, the host-side harness that parses a real .m3g out
 *     of a game JAR without a PSP in the loop.
 *
 * Handles are returned as M3GObject (a pointer).  The Java side stores them in
 * an int field, which is sound here because inc/m3g_defs.h:609 already asserts
 * sizeof(M3Guint) >= sizeof(void*) for every configuration m3gcore supports.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#ifndef __M3G_PSP_H__
#define __M3G_PSP_H__

#include "M3G/m3g_core.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
 * Failure codes
 *
 * Returned by m3gPspLoadFromMemory in place of an object count, so they are
 * all negative.  They are mirrored by the M3G_LOAD_* constants in
 * javax.microedition.m3g.Loader, which turns them into the exception the
 * JSR-184 spec asks for.
 *--------------------------------------------------------------------*/

#define M3G_PSP_ERR_INVALID       (-1)  /* null/empty input               */
#define M3G_PSP_ERR_NO_INTERFACE  (-2)  /* m3gCreateInterface failed      */
#define M3G_PSP_ERR_OUT_OF_MEMORY (-3)  /* engine reported OOM            */
#define M3G_PSP_ERR_IO            (-4)  /* malformed or truncated file    */
#define M3G_PSP_ERR_UNSUPPORTED   (-5)  /* PNG identifier, or no roots    */

/*!
 * \brief The process-wide M3G interface, created on first use.
 *
 * There is deliberately one for the whole VM: engine objects belong to the
 * interface that created them, and objects from two different interfaces
 * cannot be linked into the same scene graph.
 *
 * \return the interface, or NULL if it could not be created
 */
M3GInterface m3gPspGetInterface(void);

/*!
 * \brief Parses one .m3g image out of memory.
 *
 * \param data    the file bytes
 * \param length  how many of them
 * \param objects receives a newly allocated array of the root (unreferenced)
 *                objects, each with one reference held on the caller's behalf.
 *                Release with m3gPspReleaseRoots.
 *
 * \return the number of roots (>= 0), or one of M3G_PSP_ERR_* on failure, in
 *         which case *objects is set to NULL and nothing needs releasing.
 */
M3Gint m3gPspLoadFromMemory(const M3Gubyte *data,
                            M3Gsizei length,
                            M3GObject **objects);

/*!
 * \brief Drops the references m3gPspLoadFromMemory took and frees the array.
 *
 * Pass the count it returned.  Every root goes to refcount zero and takes the
 * scene graph hanging off it along, so this is the "the load was for nothing"
 * path.
 */
void m3gPspReleaseRoots(M3GObject *objects, M3Gint count);

/*!
 * \brief Frees the array but keeps the objects alive.
 *
 * The counterpart of m3gPspReleaseRoots for when ownership of the references
 * has been handed to somebody else -- in practice, to the Java wrappers the
 * KNI layer built around the handles.  Freeing here rather than in the caller
 * keeps the malloc and the free in the same translation unit.
 */
void m3gPspFreeRootArray(M3GObject *objects);

/*! \brief m3gGetClass, but tolerant of a NULL handle (returns -1). */
M3Gint m3gPspGetClassID(M3GObject object);

#if defined(__cplusplus)
}
#endif

#endif /* __M3G_PSP_H__ */
