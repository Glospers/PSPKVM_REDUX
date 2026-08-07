/*
 * amr_to_wave -- AMR-NB file container handling around FFmpeg's decoder.
 *
 * The decoder itself (src/ffmpeg/amrnbdec.c) speaks in single 20 ms frames
 * and knows nothing about files. This file supplies what surrounds it: the
 * storage-format header, the per-frame table of contents, the planar-float
 * to 16-bit conversion, and a WAVE wrapper so the result can go straight
 * into the existing player path.
 *
 * Storage format is RFC 4867 section 5: a magic, then for each frame one
 * table-of-contents byte whose bits 3..6 give the mode, followed by that
 * mode's payload. Frame sizes are fixed per mode, so no bitstream parsing
 * is needed to walk the file.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>       /* lrintf, to round exactly as FFmpeg's own conversion does */

#include "amr_to_wave.h"

#include "avcodec.h"
#include "codec_internal.h"

/*
 * The decoder's entry points and its private-context size are all static to
 * amrnbdec.c -- but that file ends by exporting them in its FFCodec
 * registration literal, which is precisely what the literal is for. Going
 * through it means the vendored source stays byte-for-byte upstream, so
 * pulling a newer FFmpeg is a copy rather than a merge.
 */
extern const FFCodec ff_amrnb_decoder;

#define AMR_MAGIC        "#!AMR\n"
#define AMR_MAGIC_LEN    6
#define AMR_BLOCK        160          /* samples per 20 ms frame at 8 kHz  */
#define AMR_RATE         8000
#define AMR_N_MODES      9

/* Payload bytes per mode, excluding the table-of-contents byte.
 * Modes 0..7 are the speech rates, 8 is comfort noise (SID). */
static const unsigned char amr_frame_bytes[AMR_N_MODES] = {
    12, 13, 15, 17, 19, 20, 26, 31, 5
};

int amr_is_amr(const void *data, size_t length)
{
    return data != NULL
        && length >= AMR_MAGIC_LEN
        && memcmp(data, AMR_MAGIC, AMR_MAGIC_LEN) == 0;
}

static void wave_put32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char) (v      );
    p[1] = (unsigned char) (v >>  8);
    p[2] = (unsigned char) (v >> 16);
    p[3] = (unsigned char) (v >> 24);
}

static void wave_put16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char) (v     );
    p[1] = (unsigned char) (v >> 8);
}

/* Canonical 44-byte PCM header. Written by hand because pulling in a WAVE
 * writer for one fixed layout would be more code than this. */
static void wave_header(unsigned char *h, unsigned int data_bytes)
{
    const unsigned int rate     = AMR_RATE;
    const unsigned int channels = 1;
    const unsigned int bits     = 16;
    const unsigned int align    = channels * bits / 8;

    memcpy(h, "RIFF", 4);
    wave_put32(h + 4, 36 + data_bytes);
    memcpy(h + 8, "WAVEfmt ", 8);
    wave_put32(h + 16, 16);                    /* fmt chunk size          */
    wave_put16(h + 20, 1);                     /* PCM                     */
    wave_put16(h + 22, channels);
    wave_put32(h + 24, rate);
    wave_put32(h + 28, rate * align);          /* byte rate               */
    wave_put16(h + 32, align);
    wave_put16(h + 34, bits);
    memcpy(h + 36, "data", 4);
    wave_put32(h + 40, data_bytes);
}

void *amr_to_wave(const void *data, size_t length, size_t *out_size)
{
    const unsigned char *in  = (const unsigned char *) data;
    const unsigned char *end;
    AVCodecContext       avctx;
    AVFrame              frame;
    float                plane[AMR_BLOCK];
    unsigned char       *planes[1];
    void                *priv    = NULL;
    unsigned char       *out     = NULL;
    size_t               cap     = 0;
    size_t               used    = 0;   /* PCM bytes written after the header */
    int                  ok      = 0;

    if (!amr_is_amr(in, length) || out_size == NULL) {
        return NULL;
    }
    end = in + length;
    in += AMR_MAGIC_LEN;

    priv = calloc(1, (size_t) ff_amrnb_decoder.priv_data_size);
    if (priv == NULL) {
        return NULL;
    }

    memset(&avctx, 0, sizeof(avctx));
    avctx.priv_data = priv;
    if (ff_amrnb_decoder.init(&avctx) < 0) {
        free(priv);
        return NULL;
    }

    /* Start with the header in place; PCM is appended after it, so the
     * buffer handed back is already the complete WAVE image. */
    cap = 44 + 64 * 1024;
    out = (unsigned char *) malloc(cap);
    if (out == NULL) {
        free(priv);
        return NULL;
    }

    memset(&frame, 0, sizeof(frame));
    planes[0]           = (unsigned char *) plane;
    frame.extended_data = planes;
    frame.nb_samples    = AMR_BLOCK;

    while (in < end) {
        AVPacket pkt;
        int      got   = 0;
        int      mode  = (in[0] >> 3) & 0x0F;
        size_t   avail = (size_t) (end - in);
        size_t   need;
        int      i, consumed;

        /* Modes 9..14 are reserved and 15 is "no data"; either way there is
         * no payload to walk past, so the file cannot be followed further. */
        if (mode >= AMR_N_MODES) {
            break;
        }
        need = 1 + (size_t) amr_frame_bytes[mode];
        if (avail < need) {
            break;
        }

        pkt.data = in;
        pkt.size = (int) need;

        consumed = ff_amrnb_decoder.cb.decode(&avctx, &frame, &got, &pkt);
        if (consumed < 0) {
            break;
        }

        if (got) {
            if (used + 44 + AMR_BLOCK * 2 > cap) {
                unsigned char *bigger;
                size_t         want = cap * 2;
                bigger = (unsigned char *) realloc(out, want);
                if (bigger == NULL) {
                    break;
                }
                out = bigger;
                cap = want;
            }

            /* Planar float to signed 16-bit, matching how FFmpeg's own
             * conversion does it: scale by 1<<15, round to nearest, then
             * clamp. Truncating instead, or scaling by 32767, leaves the
             * output a least-significant bit away from the reference on
             * roughly a quarter of all samples. Saturation matters too --
             * the decoder overshoots slightly on loud frames, and wrapping
             * there is an audible click rather than a clipped peak. */
            for (i = 0; i < AMR_BLOCK; i++) {
                long v = lrintf(plane[i] * 32768.0f);

                if (v >  32767) v =  32767;
                if (v < -32768) v = -32768;
                wave_put16(out + 44 + used, (unsigned int) (v & 0xFFFF));
                used += 2;
            }
            ok = 1;
        }

        in += need;
    }

    free(priv);

    if (!ok || used == 0) {
        free(out);
        return NULL;
    }

    wave_header(out, (unsigned int) used);
    *out_size = 44 + used;
    return out;
}
