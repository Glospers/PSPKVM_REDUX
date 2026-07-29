/*
 * m3g_loader_kni.c -- the natives behind javax.microedition.m3g.Loader.
 *
 * phoneME binds natives statically: the romizer writes the mangled symbol name
 * straight into ROMImage.cpp (cldc/src/vm/share/ROM/SourceObjectWriter.cpp:648,
 * via Natives::convert_to_jni_name) and jcc's CLDC_HI_NativesWriter emits the
 * same names into nativeFunctionTable.cpp for the non-romized path.  Both
 * derive the name from the class and method alone, so the only thing this file
 * has to get right is the spelling below -- there is no registration call and
 * nothing to keep in sync by hand.  A KNI native takes no C arguments; the Java
 * ones are fetched by 1-based index (for a static method, index 1 is the first
 * parameter -- cldc/src/vm/share/natives/PCSLSocket.cpp:161 is the model this
 * file follows).
 *
 * The engine work itself is in m3g/src/m3g_psp_loader.c; this is only the
 * marshalling.  A load spans three natives -- nLoadData, then nResultHandle /
 * nResultClass per root, then nResultCommit or nResultAbort -- because KNI has
 * no way to hand an int[] back that the native side allocated.  The result
 * therefore sits in a static between the calls, and Loader.decode() holds a
 * monitor across the whole sequence.
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
 * The pending load result
 *--------------------------------------------------------------------*/

static M3GObject *s_roots     = NULL;
static M3Gint     s_rootCount = 0;

/*----------------------------------------------------------------------
 * Arena reporting
 *
 * m3gcore allocates from a private static heap rather than the C heap, because
 * on this platform the C heap contains the Java object heap and an engine
 * overrun would land in VM metadata (see inc/M3G/m3g_psp.h and
 * m3g/src/m3g_psp_arena.c). The arena is checked after every load; this writes
 * what it found to ms0:/pspkvm_vm.log, the same sink the interpreter's
 * null-constant-pool guard uses (docker/patches/0043).
 *
 * Declared weak so a build without that patch -- and the romgen host tool,
 * which never has javacall -- still links; there the symbol resolves to 0 and
 * the report is skipped.
 *--------------------------------------------------------------------*/

extern void javacall_diag_log(const char *s) __attribute__((weak));

/*!
 * \brief Writes one line per load: what the arena holds and whether it is intact.
 *
 * A clean line looks like
 *
 *   M3G: load ok roots=1 arena used=41232 peak=41232 blk=612 cap=1572736
 *
 * and a line with a non-zero corrupt= or fail= count is the whole point of the
 * exercise: corrupt= means something wrote outside a block it owns and fault=
 * says which canary caught it (M3G_PSP_ARENA_* in m3g_psp.h), fail= means the
 * arena is too small and M3G_PSP_ARENA_KB needs raising.
 */
static void m3gReportArena(jint result)
{
    M3GPspArenaStats st;
    char line[192];
    static int reportedOnce;

    if (javacall_diag_log == 0) {
        return;
    }

    m3gPspArenaGetStats(&st);

    /*
     * A title loads dozens of scenes, and javacall_diag_log costs a whole
     * open/write/close each time -- so in a quiet build this reports the first
     * load (proof that loading works at all) and thereafter only a load that
     * actually went wrong. -DM3G_TRACE restores the line-per-load behaviour,
     * which is what you want when tracking the arena's growth.
     */
#if !defined(M3G_TRACE)
    if (reportedOnce && result >= 0 && st.failures == 0 && st.corrupt == 0) {
        return;
    }
#endif
    reportedOnce = 1;

    sprintf(line,
            "M3G: load %d roots=%d arena used=%d peak=%d blk=%d cap=%d "
            "fail=%d corrupt=%d fault=%d at=0x%08x\n",
            (int) result, (int) ((result > 0) ? result : 0),
            (int) st.used, (int) st.peak, (int) st.blocks, (int) st.capacity,
            (int) st.failures, (int) st.corrupt, (int) st.fault,
            (unsigned int) st.firstBad);
    javacall_diag_log(line);
}

/*!
 * \brief Throws away a result nobody claimed, objects and all.
 *
 * Called at the start of every load so that an interrupted Java sequence
 * cannot leak a scene or leave stale handles for the next caller to read.
 */
static void m3gDiscardPendingResult(void)
{
    if (s_roots != NULL) {
        m3gPspReleaseRoots(s_roots, s_rootCount);
        s_roots = NULL;
    }
    s_rootCount = 0;
}

/*----------------------------------------------------------------------
 * Natives
 *--------------------------------------------------------------------*/

/*
 * private static native int nLoadData(byte[] data, int offset, int length);
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Loader_nLoadData()
{
    jint offset = KNI_GetParameterAsInt(2);
    jint length = KNI_GetParameterAsInt(3);
    jint result;

#if defined(M3G_TRACE)
    /* Entry marker. m3gReportArena below only runs once the parse is over, so
     * without this a load that never returns is indistinguishable from a load
     * that was never started. */
    if (javacall_diag_log != 0) {
        char begin[64];
        sprintf(begin, "M3G: load begin %d\n", (int) length);
        javacall_diag_log(begin);
    }
#endif

    m3gDiscardPendingResult();

    KNI_StartHandles(1);
    {
        KNI_DeclareHandle(dataArray);
        KNI_GetParameterAsObject(1, dataArray);

        if (KNI_IsNullHandle(dataArray)
            || offset < 0 || length <= 0
            || offset + length > KNI_GetArrayLength(dataArray)) {
            result = M3G_PSP_ERR_INVALID;
        }
        else {
            /*
             * Zero copy.  SNI_GetRawArrayPointer hands out the address of the
             * array payload inside the Java heap, which only stays put while
             * nothing can trigger a garbage collection.  m3gDecodeData
             * allocates exclusively through the M3Gparams callbacks, i.e. from
             * the C heap, and never re-enters the VM, so the array cannot move
             * before it returns.  These files run to a megabyte, so the
             * alternative -- copying into a scratch buffer -- is worth
             * avoiding on a 32 MB console.
             */
            const M3Gubyte *bytes =
                (const M3Gubyte *) SNI_GetRawArrayPointer(dataArray) + offset;

            result = (jint) m3gPspLoadFromMemory(bytes,
                                                 (M3Gsizei) length,
                                                 &s_roots);
            s_rootCount = (result > 0) ? (M3Gint) result : 0;
        }
    }
    KNI_EndHandles();

    m3gReportArena(result);

    KNI_ReturnInt(result);
}

/*
 * private static native int nResultHandle(int index);
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Loader_nResultHandle()
{
    jint index = KNI_GetParameterAsInt(1);
    jint handle = 0;

    if (s_roots != NULL && index >= 0 && index < s_rootCount) {
        handle = (jint) s_roots[index];
    }
    KNI_ReturnInt(handle);
}

/*
 * private static native int nResultClass(int index);
 */
KNIEXPORT KNI_RETURNTYPE_INT
Java_javax_microedition_m3g_Loader_nResultClass()
{
    jint index = KNI_GetParameterAsInt(1);
    jint classID = -1;

    if (s_roots != NULL && index >= 0 && index < s_rootCount) {
        classID = (jint) m3gPspGetClassID(s_roots[index]);
    }
    KNI_ReturnInt(classID);
}

/*
 * private static native void nResultCommit();
 *
 * The Java wrappers now own the references the load took, so only the handle
 * array goes.
 */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Loader_nResultCommit()
{
    m3gPspFreeRootArray(s_roots);
    s_roots = NULL;
    s_rootCount = 0;

    KNI_ReturnVoid();
}

/*
 * private static native void nResultAbort();
 */
KNIEXPORT KNI_RETURNTYPE_VOID
Java_javax_microedition_m3g_Loader_nResultAbort()
{
    m3gDiscardPendingResult();

    KNI_ReturnVoid();
}
