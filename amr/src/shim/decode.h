/* ff_get_buffer normally asks the framework to allocate the output frame.
 * Here the caller has already pointed frame->extended_data at its own
 * planar buffer before calling the decoder, so this only has to succeed. */
#ifndef AMR_SHIM_DECODE_H
#define AMR_SHIM_DECODE_H
#include "avcodec.h"
static inline int ff_get_buffer(AVCodecContext *avctx, AVFrame *frame, int flags)
{
    (void) avctx; (void) flags;
    return frame->extended_data ? 0 : -1;
}
#endif
