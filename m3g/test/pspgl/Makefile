TARGET = pspgltest
OBJS   = main.o gles1x_shim.o egl_shim.o

INCDIR   =
CFLAGS   = -O2 -G0 -Wall -std=gnu99
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS  = $(CFLAGS)

LIBDIR =
LDFLAGS =
# pspgl (libGL.a) needs pspvfpu for its matrix paths and talks to the GE
# directly via sceGe*/sceDisplay* -- it does NOT link libpspgu.
LIBS = -lGL -lpspvfpu -lpspgum -lpspgu -lpspge -lpspdisplay -lpspctrl -lpsprtc -lm

EXTRA_TARGETS   = EBOOT.PBP
PSP_EBOOT_TITLE = pspgl GLES1 test

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
