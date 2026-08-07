/* The small numeric helpers the AMR decoder and its CELP support files use.
 * Bodies follow FFmpeg's semantics exactly -- the decoder's arithmetic
 * depends on the saturation behaviour, not merely on the signatures. */
#ifndef AMR_SHIM_COMMON_H
#define AMR_SHIM_COMMON_H

/* The real libavutil/common.h pulls these in transitively, and the vendored
 * sources rely on that -- acelp_vectors.c calls sqrt() and
 * acelp_pitch_delay.c calls memmove() without including either header. */
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "libavutil/macros.h"

#ifndef av_cold
#  define av_cold
#endif
#ifndef av_always_inline
#  define av_always_inline inline
#endif
#ifndef av_const
#  define av_const
#endif
#ifndef av_unused
#  define av_unused
#endif
#ifndef av_restrict
#  define av_restrict
#endif

static av_always_inline int av_clip_c(int a, int amin, int amax)
{
    if      (a < amin) return amin;
    else if (a > amax) return amax;
    else               return a;
}

static av_always_inline int16_t av_clip_int16_c(int a)
{
    if ((a + 0x8000U) & ~0xFFFF) return (int16_t) ((a >> 31) ^ 0x7FFF);
    else                         return (int16_t) a;
}

static av_always_inline float av_clipf_c(float a, float amin, float amax)
{
    if      (a < amin) return amin;
    else if (a > amax) return amax;
    else               return a;
}

/* Position of the most significant set bit; av_log2(0) is 0, as in FFmpeg. */
static av_always_inline av_const int av_log2_c(unsigned int v)
{
    int n = 0;
    if (v & 0xffff0000) { v >>= 16; n += 16; }
    if (v & 0xff00)     { v >>= 8;  n += 8;  }
    if (v & 0xf0)       { v >>= 4;  n += 4;  }
    if (v & 0xc)        { v >>= 2;  n += 2;  }
    if (v & 0x2)        {           n += 1;  }
    return n;
}

#define av_clip       av_clip_c
#define av_clip_int16 av_clip_int16_c
#define av_clipf      av_clipf_c
#define av_log2       av_log2_c

#endif
