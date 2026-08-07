/* AudioDSPContext reaches this build only through acelp_pitch_delay.h's
 * declaration of ff_acelp_decode_gain_code, which the AMR-NB decoder never
 * calls (it uses ff_amr_set_fixed_gain).  The definition is dropped from the
 * vendored .c, so only the type needs to exist. */
#ifndef AMR_SHIM_AUDIODSP_H
#define AMR_SHIM_AUDIODSP_H
#include <stdint.h>
typedef struct AudioDSPContext {
    int32_t (*scalarproduct_int16)(const int16_t *v1, const int16_t *v2, int len);
} AudioDSPContext;
#endif
