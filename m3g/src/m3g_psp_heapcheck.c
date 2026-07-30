/*
 * m3g_psp_heapcheck.c -- tells us when the C heap stops being trustworthy.
 *
 * Why this exists
 * ---------------
 * A crash was traced end to end: pspgl's eglCreateContext asks for its context
 * with memalign(16, 2336), gets a non-NULL pointer back, checks it for NULL as
 * it should, and zeroes 2336 bytes at it.  The pointer it was handed was
 * 0x08E45370 -- inside our own module image, roughly 3.7 MB *below* where the
 * heap begins.  The zero-fill therefore ran across .data: the newlib heap-size
 * variable, the MIDP filename constants, and lcd.c's statics, which is what
 * left vscr_w/vscr_h at zero and made javacall_lcd_flush_partial_internal
 * divide by zero (lcd.c:380).
 *
 * Nothing in that chain is pspgl's fault, and nothing in it is the engine's:
 * the arena keeps canaries and reported them intact on every run (corrupt=0,
 * fault=0), so the damaged buffer belongs to libc, not to us.  An allocator
 * only returns an address below its own arena once its chunk metadata has been
 * overwritten -- so by the time pspgl asks for memory, the heap is already
 * broken.  What we do not know is *when* it broke, and that is the one fact
 * that would name the culprit.
 *
 * What it does
 * ------------
 * m3gPspHeapCheck(tag) asks the heap for a block the same size pspgl asks for,
 * checks whether the answer could possibly be valid, and logs the tag if it
 * could not.  The test is deliberately narrow: any pointer below _end is
 * inside the module image and cannot be heap memory under any allocator, so a
 * failure here is proof rather than suspicion.  Sprinkling the tags through
 * the phases the title passes through turns "corruption somewhere in a 9 MB
 * binary" into "the heap was still sound at X and broken by Y".
 *
 * It is a probe, not a guard: it cannot repair the heap, and it deliberately
 * does not try.  Once the culprit is found this file has served its purpose.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include "M3G/m3g_psp.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>     /* sbrk -- where the heap actually is */

/*
 * The size the C runtime was told to take for its heap, defined by
 * PSP_HEAP_SIZE_KB in psp/pspkvm.c (docker/patches/0031 sets it to 32768).
 * Signed, because the macro accepts negative values meaning "all but this much".
 */
extern int sce_newlib_heap_kb_size;

/*
 * End of .bss, supplied by the linker.  The heap cannot start below this, so
 * it is a sound lower bound whichever way the C runtime obtains its memory --
 * and on this platform that matters, because pspsdk takes the heap from a
 * kernel partition rather than growing it from _end, so the *upper* bound is
 * not knowable from the link alone.  Only the lower bound is checked, which is
 * enough: the bad pointer we are chasing lands inside the module image.
 */
extern char _end[];

/*
 * The log sink. Weak, exactly as the loader natives declare it: it is supplied
 * by the diagnostic patch on javacall's print path, and this library also links
 * into builds without it, where the probe must simply stay silent rather than
 * fail to link.
 */
extern void javacall_diag_log(const char *s) __attribute__((weak));

/* The size pspgl's eglCreateContext requests, so the probe walks the same
 * free-list buckets the real failure walked.  A tiny allocation would likely
 * come from a different bin and could easily still succeed while the bin that
 * matters is already corrupt. */
#define M3G_HEAPCHECK_SIZE 2336

/*!
 * \brief Reports the first phase in which the C heap hands back a wild pointer.
 *
 * \param tag  short label for the phase, appears verbatim in the log
 *
 * Quiet while the heap is sound, so leaving the calls in costs one allocation
 * per phase and no log traffic.  Once a phase fails it keeps reporting, since
 * knowing whether the damage is transient or permanent is worth the lines.
 */
void m3gPspHeapCheck(const char *tag)
{
    void *p;
    char line[160];
    static int reports;

    if (javacall_diag_log == 0) {
        return;
    }

    p = malloc(M3G_HEAPCHECK_SIZE);

    if (p == NULL) {
        /* Not the failure being hunted -- an honest refusal is the allocator
         * behaving correctly -- but worth a line, because it changes what the
         * caller should expect and would otherwise look like silence. */
        sprintf(line, "HEAP: %s exhausted (asked %d)\n",
                tag ? tag : "?", (int) M3G_HEAPCHECK_SIZE);
        javacall_diag_log(line);
        return;
    }

    /*
     * Capped, because the premise this test was built on turned out to be
     * false: allocations below _end are normal here, so every single call
     * reported and the log filled with a non-finding. Two lines are kept as
     * corroboration -- enough to show the addresses climb in regular steps,
     * which is what proves the allocator healthy -- and the rest are dropped.
     */
    if ((char *) p < _end && reports < 2) {
        ++reports;
        sprintf(line, "HEAP: %s p=0x%08x below _end=0x%08x\n",
                tag ? tag : "?",
                (unsigned int) (unsigned long) p,
                (unsigned int) (unsigned long) _end);
        javacall_diag_log(line);
        /* Deliberately not freed: handing a wild pointer back to the allocator
         * would corrupt it further, and this build is a diagnostic. */
        return;
    }

    free(p);
}

/*!
 * \brief Logs where the heap actually lives, once.
 *
 * The wild pointer is only recognisable as wild against real numbers, and the
 * heap's address range is not knowable from the ELF on this platform (see the
 * note on _end above).  One line at startup makes every later pointer in the
 * log readable.
 */
void m3gPspHeapReport(void)
{
    void *p;
    char line[160];

    if (javacall_diag_log == 0) {
        return;
    }

    /*
     * sbrk(0) is the current break, i.e. where the heap actually is. The probe
     * first assumed a pointer below _end had to be wild; the log said otherwise
     * -- every allocation from the very first one lands below _end and marches
     * upward in regular steps, which is a healthy allocator, not a damaged one.
     * That leaves only one reading: the heap itself sits inside the module
     * image, so ordinary mallocs write over .data. These three numbers settle
     * it, and sce_newlib_heap_kb_size is the size the C runtime was told to ask
     * for -- worth printing because it lives at 0x08E45434, inside the range the
     * heap hands out, and so is liable to be overwritten by the very heap it
     * configures.
     */
    p = malloc(M3G_HEAPCHECK_SIZE);
    sprintf(line, "HEAP: _end=0x%08x brk=0x%08x first=0x%08x kb=%d\n",
            (unsigned int) (unsigned long) _end,
            (unsigned int) (unsigned long) sbrk(0),
            (unsigned int) (unsigned long) p,
            (int) sce_newlib_heap_kb_size);
    javacall_diag_log(line);
    if (p != NULL && (char *) p >= _end) {
        free(p);
    }
}
