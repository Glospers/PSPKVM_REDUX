/* The vendored decoder ends with an FFCodec registration literal. Nothing
 * dispatches through it here -- amrnb_decode_init and amrnb_decode_frame are
 * called directly -- but it is left to compile so the file stays byte-for-byte
 * upstream and future updates remain a clean copy. */
#ifndef AMR_SHIM_CODEC_INTERNAL_H
#define AMR_SHIM_CODEC_INTERNAL_H

#include "avcodec.h"

#define CODEC_LONG_NAME(str) .p.long_name = (str)
#define FF_CODEC_DECODE_CB(func) .cb.decode = (func)

typedef struct FFCodecPublic {
    const char        *name;
    const char        *long_name;
    enum AVMediaType   type;
    enum AVCodecID     id;
    int                capabilities;
    const enum AVSampleFormat *sample_fmts;
} FFCodecPublic;

typedef struct FFCodec {
    FFCodecPublic p;
    int  priv_data_size;
    int  (*init)(AVCodecContext *);
    union {
        int (*decode)(AVCodecContext *, AVFrame *, int *, AVPacket *);
    } cb;
} FFCodec;

#endif
