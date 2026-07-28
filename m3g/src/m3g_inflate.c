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

#include "M3G/m3g_psp.h"
#include <zlib.h>

/*----------------------------------------------------------------------
 * zlib's own scratch memory
 *
 * uncompress() would allocate inflate's state and its 32 KB sliding window
 * with plain malloc, which on this platform means the C heap -- the one that
 * holds the Java object heap and has only a few hundred KB free after VM
 * startup (see inc/M3G/m3g_psp.h).  Routing zlib at the engine arena instead
 * means the whole .m3g load path, engine and decompressor alike, touches no C
 * heap at all, so nothing about loading a scene can reach VM memory or starve
 * the rest of the runtime.
 *
 * This is why the code below drives inflate by hand rather than calling
 * uncompress(): the allocator hooks live on z_stream, and uncompress() does
 * not expose one.  The sequence is otherwise identical to uncompress()'s own
 * (zlib/uncompr.c) -- inflateInit for the zlib wrapper format, one
 * inflate(Z_FINISH) over the whole buffer, inflateEnd.
 *--------------------------------------------------------------------*/

static voidpf m3gPspZAlloc(voidpf opaque, uInt items, uInt size)
{
    (void) opaque;
    return (voidpf) m3gPspArenaAlloc((M3Guint) items * (M3Guint) size);
}

static void m3gPspZFree(voidpf opaque, voidpf address)
{
    (void) opaque;
    m3gPspArenaFree((void *) address);
}

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
 *
 * avail_out is dstLength and never more, so a stream that claims to expand to
 * more than the section header promised runs out of output space and is
 * rejected rather than being allowed to write past \c dst.
 */
M3Gsizei m3gPspInflateBlock(M3Gsizei srcLength, const M3Gubyte *src,
                            M3Gsizei dstLength, M3Gubyte *dst)
{
    z_stream stream;
    int rc;

    if (src == 0 || dst == 0 || srcLength <= 0 || dstLength <= 0) {
        return 0;
    }

    stream.zalloc    = m3gPspZAlloc;
    stream.zfree     = m3gPspZFree;
    stream.opaque    = (voidpf) 0;
    stream.next_in   = (Bytef *) src;
    stream.avail_in  = (uInt) srcLength;
    stream.next_out  = (Bytef *) dst;
    stream.avail_out = (uInt) dstLength;
    stream.msg       = 0;

    if (inflateInit(&stream) != Z_OK) {
        return 0;
    }

    rc = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    /* Anything short of Z_STREAM_END means truncated input, corrupt input, or
     * more output than the section header declared.  All are section failures. */
    if (rc != Z_STREAM_END) {
        return 0;
    }
    return (M3Gsizei) stream.total_out;
}
