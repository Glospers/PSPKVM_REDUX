/* Only a mono layout is ever constructed: AMR-NB is a single-channel
 * speech codec, and the decoder reads nothing but nb_channels. */
#ifndef AMR_SHIM_CHANNEL_LAYOUT_H
#define AMR_SHIM_CHANNEL_LAYOUT_H

#include <stdint.h>
#include <string.h>

typedef struct AVChannelLayout {
    int      order;
    int      nb_channels;
    uint64_t mask;
} AVChannelLayout;

#define AV_CHANNEL_LAYOUT_MONO ((AVChannelLayout){ 1, 1, 0x1 })

static inline void av_channel_layout_uninit(AVChannelLayout *ch)
{
    if (ch) memset(ch, 0, sizeof(*ch));
}

#endif
