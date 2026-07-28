/*
 * m3g_inflate.c -- zlib decompression for the native .m3g loader.
 *
 * The .m3g file format stores its sections zlib-deflated, and the native
 * loader inflates them at src/m3g_loader.c:481 through a static wrapper called
 * m3gInflateBlock.  m3gcore does not implement the actual decompression: it
 * only routes to a platform function, which on the Symbian port is
 * m3gSymbianInflateBlock (src/m3g_loader_inflate.inl:24, implemented over
 * ezlib in src/m3g_symbian.cpp).  This file is the PSP equivalent, over the
 * zlib that the pspdev toolchain already ships (and that the EBOOT already
 * links -- see docker/patches/0017-psp-link-freetype-z-bz2.patch).
 *
 * On the build flags that get us here
 * -----------------------------------
 * src/m3g_loader.c:235 only pulls in the platform inflate hook when the build
 * is NOT M3G_TARGET_GENERIC:
 *
 *     static M3Gsizei m3gInflateBlock(...);      // :230, note: static
 *     #if !defined(M3G_TARGET_GENERIC)
 *     #   include "m3g_loader_inflate.inl"       // :236, defines it
 *     #endif
 *
 * A generic build therefore leaves m3gInflateBlock declared-but-not-defined,
 * and because it is *static* no other translation unit can supply it -- the
 * link fails with an undefined reference that cannot be fixed from outside
 * m3gcore.  Since we do not modify the m3gcore drop, the build selects
 * M3G_TARGET_SYMBIAN instead (m3g/Makefile), which is inert apart from routing
 * inflate here: its only other effects in the whole tree are a check that the
 * legacy NGL API is off (inc/m3g_defs.h:112, it is off) and two profiling
 * branches guarded by M3G_ENABLE_PROFILING (src/m3g_interface.c:1535,:1547,
 * profiling is not enabled).
 *
 * The Makefile also passes -Dm3gSymbianInflateBlock=m3gPspInflateBlock, so the
 * symbol that ends up in the ELF is named for the platform it actually runs
 * on; that macro is why the definition below is spelled m3gPspInflateBlock and
 * m3gcore's call site still resolves to it.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include "M3G/m3g_core.h"
#include <zlib.h>

/*!
 * \brief Inflates one zlib stream into a caller-supplied buffer.
 *
 * \param srcLength  bytes of compressed data at \c src
 * \param src        compressed data
 * \param dstLength  bytes available at \c dst -- the loader takes this from
 *                   the section header, so it is the exact inflated size
 * \param dst        output buffer
 * \return the number of bytes written, or 0 on failure
 *
 * Zero is the failure value the caller expects: src/m3g_loader.c:481 frees the
 * output buffer and fails the section when this returns false.
 */
M3Gsizei m3gPspInflateBlock(M3Gsizei srcLength, const M3Gubyte *src,
                            M3Gsizei dstLength, M3Gubyte *dst)
{
    uLongf outLength;
    int rc;

    if (src == 0 || dst == 0 || srcLength <= 0 || dstLength <= 0) {
        return 0;
    }

    outLength = (uLongf) dstLength;
    rc = uncompress((Bytef *) dst, &outLength,
                    (const Bytef *) src, (uLong) srcLength);

    if (rc != Z_OK) {
        return 0;
    }
    return (M3Gsizei) outLength;
}
