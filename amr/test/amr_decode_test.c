/*
 * Host harness: decode an .amr to a .wav on the desktop.
 *
 *   amr_decode_test <in.amr> <out.wav>
 *
 * The point of building this before touching the PSP is that the decoder is
 * pure arithmetic with no platform surface, so it can be proved correct
 * where there is a reference to compare against and a debugger to use.
 * Compare the output against ffmpeg's own decode of the same file:
 *
 *   ffmpeg -i in.amr -ar 8000 -ac 1 -acodec pcm_s16le ref.wav
 *   cmp ref.wav out.wav
 *
 * The two should agree sample for sample; both are the same decoder.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>

#include "amr_to_wave.h"

int main(int argc, char **argv)
{
    FILE   *f;
    void   *in, *wave;
    long    len;
    size_t  got, wlen;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <in.amr> <out.wav>\n", argv[0]);
        return 2;
    }

    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fprintf(stderr, "%s: empty\n", argv[1]); fclose(f); return 1; }

    in = malloc((size_t) len);
    if (!in) { fclose(f); return 1; }
    got = fread(in, 1, (size_t) len, f);
    fclose(f);
    if (got != (size_t) len) { fprintf(stderr, "short read\n"); free(in); return 1; }

    if (!amr_is_amr(in, got)) {
        fprintf(stderr, "%s: not an AMR-NB file (no \"#!AMR\\n\" magic)\n", argv[1]);
        free(in);
        return 1;
    }

    wave = amr_to_wave(in, got, &wlen);
    free(in);
    if (!wave) { fprintf(stderr, "decode failed\n"); return 1; }

    f = fopen(argv[2], "wb");
    if (!f) { perror(argv[2]); free(wave); return 1; }
    fwrite(wave, 1, wlen, f);
    fclose(f);

    printf("%s -> %s  (%lu bytes WAVE, %lu samples, 8000 Hz mono)\n",
           argv[1], argv[2], (unsigned long) wlen,
           (unsigned long) ((wlen - 44) / 2));
    free(wave);
    return 0;
}
