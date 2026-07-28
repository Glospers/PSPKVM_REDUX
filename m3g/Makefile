###############################################################################
# libm3g.a -- the JSR-184 (M3G) engine, built for the PSP (MIPS).
#
# Produces lib/libm3g.a out of three groups of sources:
#
#   1. m3gcore, the Nokia M3G engine (an unmodified upstream drop, fetched
#      separately and pointed at with M3GCORE_DIR).  It is a *unity build*:
#      src/m3g_core.c #includes every other .c in the engine, so the whole
#      library is one translation unit and only that one file is compiled.
#
#   2. the pieces m3gcore expects the platform to supply but that no port in
#      the drop provides for us -- src/m3g_math_compat.c (five matrix entry
#      points the engine calls but never defines) and src/m3g_inflate.c (zlib
#      decompression for the native .m3g loader) -- plus src/m3g_psp_loader.c,
#      which owns the M3GInterface and drives the loader for both the KNI
#      natives (jsr184/src/native) and the host test harness (test/).
#
#   3. src/stub/, a no-op OpenGL ES 1.1 + EGL backend.  m3gcore renders only
#      through GL ES (the software path is #error'd out at
#      src/m3g_rendercontext.inl:25) and the PSP has no GL ES, so these exist
#      to make the ELF link while the engine is being brought up.  They are
#      replaced wholesale by the sceGu backend later; nothing else changes.
#
# Usage (driven from docker/build.sh):
#
#   make -C m3g M3GCORE_DIR=/path/to/m3gcore
#
# The result lands in $(M3G_DIR)/lib and psp/Makefile picks it up from there
# (docker/patches/0042-psp-link-m3g.patch adds the -L and the -lm3g).
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
###############################################################################

M3G_DIR     := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
M3GCORE_DIR ?= $(M3G_DIR)/../m3gcore

# m3gcore is an upstream drop that is fetched separately and is not part of
# this repository, so say something useful instead of failing on a missing
# include 200 lines into the engine.
ifneq ($(MAKECMDGOALS),clean)
ifeq ($(wildcard $(M3GCORE_DIR)/src/m3g_core.c),)
$(error m3gcore sources not found at '$(M3GCORE_DIR)' -- pass M3GCORE_DIR=<path to the m3gcore checkout>)
endif
endif

OBJDIR      ?= $(M3G_DIR)/obj
LIBOUTDIR   ?= $(M3G_DIR)/lib
TARGET      := $(LIBOUTDIR)/libm3g.a

# ?= is not enough here: make has built-in defaults for CC and AR (cc, ar), so
# they are never "undefined" and ?= would leave the host compiler in place.
# Replace them only while they still hold that built-in default, which keeps a
# caller-supplied CC=... working.
ifneq ($(filter default undefined,$(origin CC)),)
CC          := psp-gcc
endif
ifneq ($(filter default undefined,$(origin AR)),)
AR          := psp-ar
endif

# -G0 is mandatory: every other object in the EBOOT is built with it
# (psp/Makefile:6) and mixing small-data thresholds fails at link.
# -fno-strict-aliasing because m3gcore type-puns object headers freely.
# -MMD -MP so that editing a header -- in particular our GLES/EGL ones, which
# shadow anything the toolchain ships -- actually rebuilds the engine.
M3G_OPT     ?= -O2
CFLAGS      := -G0 $(M3G_OPT) -fno-strict-aliasing -MMD -MP

INCLUDES    := -I$(M3G_DIR)/inc -I$(M3GCORE_DIR)/inc -I$(M3GCORE_DIR)/src

# M3G_TARGET_SYMBIAN, on a PSP, needs a word of explanation.
#
# m3gcore requires exactly one M3G_TARGET_* selector or inc/m3g_defs.h:94
# #errors.  The obvious one, M3G_TARGET_GENERIC, cannot be used here: it makes
# src/m3g_loader.c:235 skip the platform inflate hook, leaving the *static*
# m3gInflateBlock (declared at :230) with no definition and no way for another
# translation unit to supply one -- an unfixable undefined reference unless the
# m3gcore drop itself is edited, which this project does not do.
#
# M3G_TARGET_SYMBIAN takes the other branch, which forwards inflate to an
# ordinary external function we can implement (src/m3g_inflate.c, over zlib).
# Everything else the flag touches is inert in this configuration: a check that
# the legacy NGL API is off (inc/m3g_defs.h:112 -- it is) and two profiling
# branches behind M3G_ENABLE_PROFILING (src/m3g_interface.c:1535,:1547 -- not
# enabled).  Those are its only other occurrences in the whole tree.
#
# The -D on the second line renames that hook so the symbol in the PSP ELF is
# named after the platform it actually runs on.
DEFINES     := -DM3G_TARGET_SYMBIAN \
               -Dm3gSymbianInflateBlock=m3gPspInflateBlock

OBJS        := $(OBJDIR)/m3g_core.o \
               $(OBJDIR)/m3g_math_compat.o \
               $(OBJDIR)/m3g_inflate.o \
               $(OBJDIR)/m3g_psp_loader.o \
               $(OBJDIR)/m3g_gl_stub.o \
               $(OBJDIR)/m3g_egl_stub.o \
               $(OBJDIR)/m3g_native_stub.o

.PHONY: all clean

all: $(TARGET)

# 'rcs' writes the archive index, so no separate ranlib pass is needed.
$(TARGET): $(OBJS) | $(LIBOUTDIR)
	rm -f $@
	$(AR) rcs $@ $(OBJS)
	@echo "== libm3g.a =="
	@ls -la $@

# 1. the engine (unity build -- this one file is the whole of m3gcore)
$(OBJDIR)/m3g_core.o: $(M3GCORE_DIR)/src/m3g_core.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

# 2. platform pieces m3gcore expects but does not ship
$(OBJDIR)/%.o: $(M3G_DIR)/src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

# 3. the null GL ES / EGL backend
$(OBJDIR)/%.o: $(M3G_DIR)/src/stub/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

$(OBJDIR) $(LIBOUTDIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR) $(LIBOUTDIR)

-include $(OBJS:.o=.d)
