###############################################################################
# m3g_load_test -- host build of the .m3g parse check.
#
# Builds the same sources libm3g.a is made of, but with the host compiler, and
# links them against m3g_load_test.c.  The point is to be able to run a real
# .m3g file through m3gPspLoadFromMemory() -- the very function the KNI natives
# call -- on the build machine, where a failure is a stack trace away instead
# of a black PSP screen.
#
#   make -C m3g/test M3GCORE_DIR=/path/to/m3gcore
#   m3g/test/m3g_load_test /tmp/jar/data/3d/*.m3g
#
# 32-BIT HOST BUILD IS MANDATORY.  m3gcore stores object pointers in M3Guint
# fields and asserts sizeof(M3Guint) >= sizeof(void*) (m3gcore/inc/m3g_defs.h:
# 607-609); on an LP64 host that assertion is false and handles get truncated.
# Hence -m32, which also makes the run a closer match for the PSP.  On Debian
# and Ubuntu that needs gcc-multilib and the i386 zlib.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
###############################################################################

TEST_DIR    := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
M3G_DIR     := $(patsubst %/,%,$(dir $(TEST_DIR)))
M3GCORE_DIR ?= $(M3G_DIR)/../m3gcore

ifeq ($(wildcard $(M3GCORE_DIR)/src/m3g_core.c),)
$(error m3gcore sources not found at '$(M3GCORE_DIR)' -- pass M3GCORE_DIR=<path to the m3gcore checkout>)
endif

HOSTCC     ?= gcc
ARCHFLAGS  ?= -m32
# The i386 zlib runtime ships without the development symlink on Ubuntu, so
# override this with the full path to libz.so.1 if -lz does not resolve.
ZLIB       ?= -lz

OBJDIR     := $(TEST_DIR)/obj
TARGET     := $(TEST_DIR)/m3g_load_test
RENDER     := $(TEST_DIR)/m3g_render_test

INCLUDES   := -I$(M3G_DIR)/inc -I$(M3GCORE_DIR)/inc -I$(M3GCORE_DIR)/src
DEFINES    := -DM3G_TARGET_SYMBIAN -Dm3gSymbianInflateBlock=m3gPspInflateBlock
ifneq ($(M3G_PSP_ARENA_KB),)
DEFINES    += -DM3G_PSP_ARENA_KB=$(M3G_PSP_ARENA_KB)
endif
CFLAGS     := $(ARCHFLAGS) -O1 -g -fno-strict-aliasing -w

OBJS       := $(OBJDIR)/m3g_core.o \
              $(OBJDIR)/m3g_math_compat.o \
              $(OBJDIR)/m3g_inflate.o \
              $(OBJDIR)/m3g_psp_arena.o \
              $(OBJDIR)/m3g_psp_loader.o \
              $(OBJDIR)/m3g_gl_stub.o \
              $(OBJDIR)/m3g_egl_stub.o \
              $(OBJDIR)/m3g_native_stub.o \
              $(OBJDIR)/m3g_psp_platform_stub.o \
              $(OBJDIR)/m3g_load_test.o

RENDER_OBJS := $(filter-out $(OBJDIR)/m3g_load_test.o,$(OBJS)) \
               $(OBJDIR)/m3g_psp_render.o \
               $(OBJDIR)/m3g_render_test.o

.PHONY: all clean

all: $(TARGET) $(RENDER)

$(TARGET): $(OBJS)
	$(HOSTCC) $(ARCHFLAGS) -o $@ $(OBJS) $(ZLIB) -lm

$(RENDER): $(RENDER_OBJS)
	$(HOSTCC) $(ARCHFLAGS) -o $@ $(RENDER_OBJS) $(ZLIB) -lm

$(OBJDIR)/m3g_core.o: $(M3GCORE_DIR)/src/m3g_core.c | $(OBJDIR)
	$(HOSTCC) $(CFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

$(OBJDIR)/%.o: $(M3G_DIR)/src/%.c | $(OBJDIR)
	$(HOSTCC) $(CFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

$(OBJDIR)/%.o: $(TEST_DIR)/stub/%.c | $(OBJDIR)
	$(HOSTCC) $(CFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

$(OBJDIR)/%.o: $(TEST_DIR)/%.c | $(OBJDIR)
	$(HOSTCC) $(CFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

$(OBJDIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR) $(TARGET)
