/* Assertions compile out: a malformed frame must degrade to silence on a
 * handheld, never abort the VM. */
#ifndef AMR_SHIM_AVASSERT_H
#define AMR_SHIM_AVASSERT_H
#define av_assert0(cond) do { } while (0)
#define av_assert1(cond) do { } while (0)
#define av_assert2(cond) do { } while (0)
#endif
