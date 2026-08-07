/* The decoder logs only on malformed input.  Routed to stderr on the host
 * harness and compiled out on the PSP, where there is no console. */
#ifndef AMR_SHIM_LOG_H
#define AMR_SHIM_LOG_H
#define AV_LOG_ERROR   16
#define AV_LOG_WARNING 24
#define AV_LOG_INFO    32
#ifdef AMR_SHIM_QUIET
#  define av_log(ctx, level, ...) do { } while (0)
#else
#  include <stdio.h>
#  define av_log(ctx, level, ...) fprintf(stderr, __VA_ARGS__)
#endif
#define avpriv_report_missing_feature(ctx, ...) do { } while (0)
#endif
