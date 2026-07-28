TARGET = pspglcoex
OBJS   = main.o ../gles1x_shim.o ../egl_shim.o

INCDIR   = ..
CFLAGS   = -O2 -G0 -Wall -std=gnu99
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS  = $(CFLAGS)

LIBS = -lGL -lpspvfpu -lpspgum -lpspgu -lpspge -lpspdisplay -lpspctrl -lm

EXTRA_TARGETS   = EBOOT.PBP
PSP_EBOOT_TITLE = pspgl + sceGu coexistence

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
