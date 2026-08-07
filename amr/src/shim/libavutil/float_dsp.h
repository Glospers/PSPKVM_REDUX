/*
 * Two vendored files include this header, and both call
 * avpriv_scalarproduct_float_c, which lives in FFmpeg's float_dsp.c -- a
 * file that is otherwise entirely SIMD dispatch and not worth vendoring for
 * one dot product. The body below is that function's generic C path.
 *
 * Nothing here references AVFloatDSPContext; neither caller uses it.
 */
#ifndef AMR_SHIM_FLOAT_DSP_H
#define AMR_SHIM_FLOAT_DSP_H

static inline float avpriv_scalarproduct_float_c(const float *v1,
                                                 const float *v2, int len)
{
    float p = 0.0f;
    int i;

    for (i = 0; i < len; i++) {
        p += v1[i] * v2[i];
    }
    return p;
}

#endif /* AMR_SHIM_FLOAT_DSP_H */
