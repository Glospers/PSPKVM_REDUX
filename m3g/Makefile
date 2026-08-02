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
#      natives (jsr184/src/native) and the host test harness (test/), and
#      src/m3g_psp_arena.c, the engine's private heap.  The engine must not
#      allocate from the C heap on this platform: the Java object heap is a
#      malloc'd block inside it, so engine blocks would sit against VM memory.
#      Pass M3G_PSP_ARENA_KB=<n> to resize that heap.
#
#   3. the OpenGL ES 1.x backend.  m3gcore renders only through GL ES (the
#      software path is #error'd out at src/m3g_rendercontext.inl:25), and on
#      the PSP that is pspgl -- libGL.a, BSD-3-Clause, already in the pspdev
#      toolchain, driving the GE through its own display lists rather than
#      libpspgu.  It covers all but sixteen of the entry points m3gcore uses;
#      src/m3g_psp_gl.c and src/m3g_psp_egl.c supply those, src/m3g_psp_native.c
#      refuses the native bitmap/window hooks the PSP has no handle space for,
#      and src/m3g_psp_vidmem.c keeps pspgl's video memory allocator out of the
#      edram PSPKVM already owns.  The -lGL itself is added at the ELF link by
#      docker/patches/0044.
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
#
# M3G_FORCE_BUFFERED_RENDERING is the engine's own switch (checked at
# src/m3g_rendercontext.c:583) for "never render straight at the target".  It
# is mandatory here.  The target is always a memory target -- MIDP's 16-bit
# screen buffer, see src/m3g_psp_render.c -- and m3gValidateBuffers otherwise
# decides between direct and buffered rendering by asking EGL whether the
# format is renderable (m3gCanDirectRender, src/m3g_rendercontext.inl:746).
# pspgl's eglChooseConfig ignores EGL_SURFACE_TYPE, so it answers yes for a
# memory target -- for which m3gSelectGLSurface has no case at all (:1296,
# default: assert and return NULL).  Forcing the buffered path takes the
# decision away from EGL and is what makes the pbuffer + glReadPixels route
# run.
DEFINES     := -DM3G_TARGET_SYMBIAN \
               -Dm3gSymbianInflateBlock=m3gPspInflateBlock \
               -DM3G_FORCE_BUFFERED_RENDERING \
               -DM3G_NUM_TEXTURE_UNITS=1

# M3G_NUM_TEXTURE_UNITS=1 because the GE has one texture unit and the GL shim
# necessarily makes glActiveTexture a no-op.  m3gcore's default is 2, and its
# per-unit appearance setup then ENABLES the real unit as "unit 0" and
# immediately DISABLES it again as "unit 1" -- with a no-op ActiveTexture both
# land on the same hardware, so every textured mesh drew untextured (found via
# the host trace: enable/bind/upload followed by disable before every draw).

# TEMPORARY -- one-shot dump of an early frame's raw 565 read-back, to place
# the white-frame fault on one side or the other of the format conversion.
# See the M3G_PSP_READBACK_DUMP block in src/m3g_psp_gl.c.
# DEFINES   += -DM3G_PSP_READBACK_DUMP   (diagnostic, off)

# TEMPORARY -- clear the scene to dark blue each frame so geometry cannot hide
# behind the accumulated buffer. See m3gPspClear in src/m3g_psp_render.c.
# DEFINES   += -DM3G_PSP_SCENE_BACKDROP   (diagnostic, off)

# TEMPORARY -- never enable face culling, to test whether the GL-to-GE flip
# inverts winding and the whole scene is being culled as back faces. See
# __wrap_glEnable in src/m3g_psp_gl.c.  (Acquitted: the screen stayed flat
# blue with culling off. Kept while the raster probe below runs.)
# DEFINES   += -DM3G_PSP_NO_CULL   (diagnostic, off)

# TEMPORARY -- draw one yellow triangle through raw GL each frame, the same
# usage the spinning-cube demo proved. Separates "pspgl cannot draw into this
# pbuffer" from "the engine's draws are malformed". See m3gPspClear in
# src/m3g_psp_render.c.
# DEFINES   += -DM3G_PSP_PROBE_TRIANGLE   (diagnostic, off)

# The video memory budget, sized so the render target can never be evicted.
#
# The back buffer is a 512x512 pbuffer (pspgl only makes power-of-two
# surfaces; the EGL wrapper in src/m3g_psp_egl.c rounds the engine's 480x272
# request up) in 5650 with a depth buffer: 512 KB + 512 KB = 1024 KB, and it
# must live in edram -- the GE happily reports success rasterising into main
# memory, but nothing CPU-visible ever appears there, which was one layer of
# the black-screen problem.
#
# The reservation leaves pspgl 1048 KB: the 512x512 pbuffer pair (1024) fits
# with 24 KB to spare, and that spare is DELIBERATELY too small for any
# texture.  The 848 experiment (1200 KB share, 176 KB spare) let small
# textures into edram, and pspgl's buffer eviction scribbled over live
# texture content: noise-static faces on the station, and texture swaps
# mid-frame.  It was chasing a depth-in-RAM theory the placement log later
# disproved -- the measured drop was the full 1024 KB, both buffers in
# edram even at the old size.  Textures belong in system memory on this
# stack; the GE reads them there perfectly well.
DEFINES     += -DM3G_PSP_VRAM_RESERVE_KB=1000

# Reserve only the edram PSPKVM actually holds, not all of it.
#
# src/m3g_psp_vidmem.c clamps this up to getStaticVramUsed(), so 0 means
# "exactly PSPKVM's high-water mark and not a byte more", leaving pspgl the
# remaining ~704 KB of edram to allocate in.
#
# It used to reserve everything.  That was not because pspgl needed keeping out
# of edram -- the watermark alone does that -- but to stop it ever taking the
# eviction path, which copies a buffer out with the GE and dereferences pspgl's
# current-context global while doing it.  With no context current that global
# is NULL and the copy faults.  The port now holds a context for the life of
# the process (m3gPspHoldGLContext, src/m3g_psp_egl.c), so that precondition is
# gone and eviction has a context to work with.
#
# What it buys: with everything reserved, every pspgl buffer fell back to
# memalign from the C heap -- which on this platform is where the Java object
# heap and PSPKVM's own display globals live.  Handing pspgl real video memory
# takes it out of that heap altogether.
#
# If this reintroduces a fault inside pspgl's eviction path, the held context
# is not doing what it should and that is a finding about pspgl, not something
# to paper over: raise this back to a value large enough to cover edram.
# Removed now that the heap is fixed.
#
# This was set to 0 -- give pspgl all of the video memory -- to keep its buffers
# out of the C heap, because the C heap was the thing that could not be trusted:
# it overlapped .data, so anything allocated from it wrote over the program. That
# is fixed at the source now (heap taken from the top of the partition, see
# psp/pspkvm.c), so the reason for handing pspgl video memory has gone, and what
# is left is the cost of doing it: pspgl and PSPKVM's blit both drive the GE, and
# PSPKVM keeps its display buffers in video memory below the watermark.
#
# The symptom that matters -- documented against m3gIface in
# jsr184/src/native/m3g_object_kni.c:210 as "the MIDlet freezes on its loading
# screen with even the background animation stopped" -- is what the title does
# now: the animation runs while it loads, then stops, and no target is ever
# bound. Reverting to the default reserves video memory for the display and puts
# pspgl's allocations back on a heap that is finally sound.
#
# DEFINES   += -DM3G_PSP_VRAM_RESERVE_KB=0

# Size of the engine's private heap (src/m3g_psp_arena.c).  Left unset the
# source picks its own default; override on the command line to retune after
# reading the peak= figure the KNI layer logs to ms0:/pspkvm_vm.log.
# 768: the source default of 384 refused one block during Deep 3D's race
# loads (fail=1 with ~106K free means a single allocation past 106K -- the
# track mesh), and the race rendered as fog over nothing.  Doubling it costs
# 384K of BSS, which the from-top C heap layout absorbs.
DEFINES     += -DM3G_PSP_ARENA_KB=768

ifneq ($(M3G_PSP_ARENA_KB),)
DEFINES     += -DM3G_PSP_ARENA_KB=$(M3G_PSP_ARENA_KB)
endif

OBJS        := $(OBJDIR)/m3g_core.o \
               $(OBJDIR)/m3g_math_compat.o \
               $(OBJDIR)/m3g_inflate.o \
               $(OBJDIR)/m3g_psp_arena.o \
               $(OBJDIR)/m3g_psp_loader.o \
               $(OBJDIR)/m3g_psp_render.o \
               $(OBJDIR)/m3g_psp_vidmem.o \
               $(OBJDIR)/m3g_psp_gl.o \
               $(OBJDIR)/m3g_psp_egl.o \
               $(OBJDIR)/m3g_psp_heapcheck.o \
               $(OBJDIR)/m3g_psp_native.o

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

# 2. the platform pieces m3gcore expects but does not ship, and 3. the pspgl
#    backend.  Both compile against m3g/inc, i.e. against the same GLES/EGL
#    headers m3gcore sees rather than pspgl's -- the typedefs are all 32 bits
#    either way, so the o32 ABI is identical, and it keeps pspgl's <GLES/egl.h>
#    (which shares our include guard, __egl_h_) from ever meeting ours.
$(OBJDIR)/%.o: $(M3G_DIR)/src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

$(OBJDIR) $(LIBOUTDIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR) $(LIBOUTDIR)

-include $(OBJS:.o=.d)
