#ifndef AMR_SHIM_MACROS_H
#define AMR_SHIM_MACROS_H
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFSWAP(type,a,b) do { type SWAP_tmp = b; b = a; a = SWAP_tmp; } while (0)
#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#endif
