/* Build configuration for the vendored FFmpeg AMR-NB decoder.
 *
 * The generic C paths are selected deliberately.  FFmpeg's MIPS routines
 * target MIPS32r2 with the DSP ASE; the PSP's Allegrex core does not
 * implement that set, so ARCH_MIPS stays off and lsp.c uses portable C.
 */
#ifndef AMR_SHIM_CONFIG_H
#define AMR_SHIM_CONFIG_H
#define ARCH_MIPS     0
#define HAVE_MIPSFPU  0
#endif
