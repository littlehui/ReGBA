TARGET      := regba
RG350=1
CROSS_COMPILE ?= mipsel-gcw0-linux-uclibc-
CC          := $(CROSS_COMPILE)gcc
STRIP       := $(CROSS_COMPILE)strip

SYSROOT     := $(shell $(CC) --print-sysroot)
SDL_CFLAGS  := $(shell $(SYSROOT)/usr/bin/sdl-config --cflags)
SDL_LIBS    := $(shell $(SYSROOT)/usr/bin/sdl-config --libs)

OBJS        := main.o draw.o port.o port-asm.o od-input.o ../video.o          \
              ../input.o ../bios.o ../zip.o ../sound.o ../mips/stub.o         \
              ../stats.o ../memory.o ../cpu_common.o ../cpu_asm.o od-sound.o  \
              ../sha1.o imageio.o ../unifont.o gui.o od-memory.o settings.o   \
              cheats.o charsets.o ../module/obj/*.o cc_lut.o
           
HEADERS     := cheats.h ../common.h ../cpu_common.h ../cpu.h draw.h main.h    \
               ../input.h ../memory.h message.h ../mips/emit.h ../sound.h     \
               ../stats.h ../video.h ../zip.h port.h od-sound.h ../sha1.h     \
               imageio.h ../unifont.h od-input.h settings.h charsets.h ../module/obj/*.h cc_lut.h

INCLUDE     := -I. -I.. -I../mips -I../module/obj
OPTIMIZE =  -O2 -mno-mips16 -fomit-frame-pointer -fno-builtin   \
            -fno-common -Wno-write-strings -Wno-sign-compare -ffast-math -ftree-vectorize \
			-funswitch-loops -fno-strict-aliasing
			
DEFS        := -DGCW_ZERO -DMIPS_XBURST                                       \
               -DSCREEN_WIDTH=320 -DSCREEN_HEIGHT=240                         \
               -DGIT_VERSION=$(shell git describe --always)

ifdef RG350
	DEFS += -DRUMBLE
	DEFS += -DRG350
	DEFS += -DUSE_MMAP
else
	DEFS += -DLOAD_ALL_ROM
endif
HAS_MIPS32R2 := $(shell echo | $(CC) -dM -E - |grep _MIPS_ARCH_MIPS32R2)
ifneq ($(HAS_MIPS32R2),)
	DEFS += -DMIPS_32R2
	OPTIMIZE += -mips32r2
else
	OPTIMIZE += -mips32 -march=mips32
endif

CFLAGS      := $(SDL_CFLAGS) -mno-abicalls -Wall -Wno-unused-variable \
               -fomit-frame-pointer $(DEFS) $(INCLUDE) $(OPTIMIZE)
ASFLAGS     := $(CFLAGS) -D__ASSEMBLY__
##LDFLAGS     := $(SDL_LIBS) -lpthread -lz -lm -lpng -lshake -s
LDFLAGS     := $(SDL_LIBS) -lpthread -lz -lm -lpng  -s -w

DATA_TO_CLEAN := .opk_data $(TARGET).opk

include ../Makefile.rules

.PHONY: all opk

all: $(TARGET)

opk: $(TARGET).opk

$(TARGET).opk: $(TARGET)
	$(SUM) "  OPK     $@"
	$(CMD)rm -rf .opk_data
	$(CMD)cp -r data .opk_data
	$(CMD)cp ../../game_config.txt .opk_data
	$(CMD)cp ../../bios/gba_bios.bin .opk_data
	$(CMD)cp $< .opk_data/regba
	$(CMD)$(STRIP) .opk_data/regba
	$(CMD)mksquashfs .opk_data $@ -all-root -noappend -no-exports -no-xattrs -no-progress >/dev/null

# The two below declarations ensure that editing a .c file recompiles only that
# file, but editing a .h file recompiles everything.
# Courtesy of Maarten ter Huurne.

# Each object file depends on its corresponding source file.
$(C_OBJS): %.o: %.c

# Object files all depend on all the headers.
$(OBJS): $(HEADERS)
