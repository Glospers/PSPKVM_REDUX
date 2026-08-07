#ifndef AMR_SHIM_MATHOPS_H
#define AMR_SHIM_MATHOPS_H
#include "libavutil/common.h"
#define MULL(a,b,s) (((int64_t)(a) * (int64_t)(b)) >> (s))
#define MUL16(a,b)  ((a) * (b))
#define MAC16(rt,ra,rb) rt += (ra) * (rb)
#endif
