#ifndef AMR_SHIM_FFMATH_H
#define AMR_SHIM_FFMATH_H
#include <math.h>
/* FFmpeg uses a fast approximation here; exp2f is exact and the decoder is
 * not hot enough on this target to justify the approximation's error. */
static inline float ff_exp10f(float x) { return powf(10.0f, x); }
static inline double ff_exp10(double x) { return pow(10.0, x); }
static inline float ff_exp2fi(int x) { return ldexpf(1.0f, x); }
#endif
