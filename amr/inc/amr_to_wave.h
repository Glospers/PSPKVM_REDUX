/*
 * amr_to_wave -- decode an AMR-NB file to a RIFF/WAVE image in memory.
 *
 * The runtime's media layer already knows how to play WAVE bytes: it hands
 * a memory buffer to SDL_mixer and gets a chunk back. So the smallest
 * correct place to add AMR support is to turn the AMR bytes into WAVE bytes
 * before that hand-off, which leaves the player, the mixer, the channel
 * bookkeeping and the MIDlet's own resource lookups completely untouched.
 *
 * This replaces the per-title workaround of transcoding a suite's .amr
 * entries on the desktop before installing it: a JAR downloaded from
 * anywhere now plays its effects as it stands.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 */
#ifndef AMR_TO_WAVE_H
#define AMR_TO_WAVE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * True if the buffer opens with an AMR-NB single-channel magic.
 *
 * Cheap enough to call on every clip the media layer buffers, which is how
 * the decoder stays out of the way of ordinary WAVE content.
 */
int amr_is_amr(const void *data, size_t length);

/**
 * Decode AMR-NB bytes to a newly allocated RIFF/WAVE image.
 *
 * The result is 8 kHz mono signed 16-bit PCM -- the codec's native output
 * rate, left alone so the mixer does the one resample it was going to do
 * anyway.
 *
 * @param data      AMR file bytes, including the "#!AMR\n" magic
 * @param length    number of bytes at data
 * @param out_size  receives the size of the returned image
 * @return          malloc'd WAVE image the caller frees, or NULL if the
 *                  input is not decodable
 */
void *amr_to_wave(const void *data, size_t length, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* AMR_TO_WAVE_H */
