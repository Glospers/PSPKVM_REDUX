/* The decoder returns these on malformed input. Values match FFmpeg's so a
 * caller comparing against upstream constants sees the same numbers; all
 * that matters locally is that they are negative. */
#ifndef AMR_SHIM_ERROR_H
#define AMR_SHIM_ERROR_H

#define AVERROR(e) (-(e))
#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#define FFERRTAG(a,b,c,d) (-(int)MKTAG(a,b,c,d))

#define AVERROR_INVALIDDATA  FFERRTAG('I','N','D','A')
#define AVERROR_PATCHWELCOME FFERRTAG(0xF8,'P','A','W')

#endif
