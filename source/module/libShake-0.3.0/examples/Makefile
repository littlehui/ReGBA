ifeq ($(PLATFORM), gcw0)
  CC         := /opt/gcw0-toolchain/usr/bin/mipsel-linux-gcc
  STRIP      := /opt/gcw0-toolchain/usr/bin/mipsel-linux-strip
endif

CC           ?= gcc
STRIP        ?= strip
TARGET       ?= simple listDevices
LDFLAGS	     := -L.. -lshake
CFLAGS       := -fPIC -I../include
SRCDIR       := .

ifdef DEBUG
  CFLAGS += -ggdb -Wall -Werror -pedantic -std=c99
else
  CFLAGS += -O2
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) $(SRCDIR)/$@.c $(LDFLAGS) -o $@
	$(STRIP) $@

clean:
	rm -Rf $(TARGET)
