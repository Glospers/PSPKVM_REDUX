#
# AMR-NB decoder: static library for the PSP, plus a host test harness.
#
#   make            -- lib/libamr.a for the PSP (psp-gcc)
#   make host-test  -- test/amr_decode_test on the build machine (cc)
#
# The same sources build both ways on purpose: the decoder is pure
# arithmetic with no platform surface, so it can be proved on the host
# against ffmpeg's own output before it ever runs on hardware.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
#

TARGET   = lib/libamr.a
SRCDIR   = src
FFDIR    = $(SRCDIR)/ffmpeg
OBJDIR   = obj

# The shim directory must come first: it supplies avcodec.h, config.h and the
# libavutil/ headers the vendored sources include, standing in for a full
# FFmpeg tree that is not present.
INCLUDES = -I$(SRCDIR)/shim -Iinc -I$(FFDIR)

SOURCES  = $(SRCDIR)/amr_to_wave.c \
           $(FFDIR)/amrnbdec.c \
           $(FFDIR)/celp_math.c \
           $(FFDIR)/celp_filters.c \
           $(FFDIR)/acelp_filters.c \
           $(FFDIR)/acelp_vectors.c \
           $(FFDIR)/acelp_pitch_delay.c \
           $(FFDIR)/lsp.c

OBJECTS  = $(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(SOURCES)))
vpath %.c $(SRCDIR) $(FFDIR)

PSP_CC   ?= psp-gcc
PSP_AR   ?= psp-ar

# -G0 keeps everything out of the small-data area, matching how the rest of
# the PSP build is compiled; the tables here are far larger than $gp reach.
# AMR_SHIM_QUIET drops the decoder's error logging, which has nowhere to go
# on a handheld.
PSP_CFLAGS = -O2 -G0 -fno-strict-aliasing -DAMR_SHIM_QUIET $(INCLUDES)

all: $(TARGET)

$(OBJDIR):
	mkdir -p $(OBJDIR)

lib:
	mkdir -p lib

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(PSP_CC) $(PSP_CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) | lib
	$(PSP_AR) rcs $@ $(OBJECTS)
	@echo "built $@"

# -------------------------------------------------------------------------
# Host harness. Error logging is left on here: on the desktop it is the
# fastest way to see why a malformed file was rejected.
# -------------------------------------------------------------------------
HOST_CC     ?= cc
HOST_CFLAGS  = -O2 -g -fno-strict-aliasing -Wall $(INCLUDES)

host-test: test/amr_decode_test

test/amr_decode_test: test/amr_decode_test.c $(SOURCES)
	$(HOST_CC) $(HOST_CFLAGS) -o $@ test/amr_decode_test.c $(SOURCES) -lm
	@echo "built $@"

clean:
	rm -rf $(OBJDIR) lib test/amr_decode_test

.PHONY: all clean host-test
