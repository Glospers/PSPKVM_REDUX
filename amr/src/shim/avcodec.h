/*
 * Minimal stand-in for libavcodec/avcodec.h.
 *
 * The AMR-NB decoder reaches for very little of FFmpeg's framework: four
 * fields of AVCodecContext, two of AVFrame, and a handful of enum values.
 * Everything here is what those uses require and nothing more, so the
 * vendored decoder compiles unmodified rather than being forked.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
#ifndef AMR_SHIM_AVCODEC_H
#define AMR_SHIM_AVCODEC_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "config.h"
#include "libavutil/common.h"
#include "libavutil/log.h"
#include "libavutil/avassert.h"
#include "libavutil/macros.h"

enum AVSampleFormat {
    AV_SAMPLE_FMT_NONE = -1,
    AV_SAMPLE_FMT_FLTP = 8
};

enum AVMediaType { AVMEDIA_TYPE_AUDIO = 1 };

enum AVCodecID { AV_CODEC_ID_AMR_NB = 0x12000 };

#define AV_CODEC_CAP_DR1           (1 << 1)
#define AV_CODEC_CAP_CHANNEL_CONF  (1 << 10)

#include "libavutil/channel_layout.h"
#include "libavutil/error.h"

typedef struct AVCodecContext {
    void               *priv_data;
    int                 sample_rate;
    enum AVSampleFormat sample_fmt;
    AVChannelLayout     ch_layout;
} AVCodecContext;

/* extended_data points at the caller's planar float buffer; the decoder
 * writes AMR_BLOCK_SIZE samples per channel into it. */
typedef struct AVFrame {
    uint8_t **extended_data;
    uint8_t  *data[8];
    int       nb_samples;
} AVFrame;

typedef struct AVPacket {
    const uint8_t *data;
    int            size;
} AVPacket;

#endif /* AMR_SHIM_AVCODEC_H */
