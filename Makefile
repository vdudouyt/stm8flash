# stm8flash makefile
#
# Multiplatform support
#   - Linux (x86, Raspian)
#   - MacOS (Darwin)
#   - Windows (e.g. mingw-w64-x86 with libusb1.0)


PLATFORM=$(shell uname -s)

PWD=$(shell pwd)
# Directory for Libusb - added @boseji <salearj@hotmail.com> 2017-08-04
LIBUSB_DIR=../libusb-1.0.21
# Architecture Libusb - added @boseji <salearj@hotmail.com> 2017-08-04
#  - Can be `64` or `32` only
LIBUSB_ARCH=64
# Compiler Libusb - added @boseji <salearj@hotmail.com> 2017-08-04
#  - Can be `MinGW` for mingw-w64-x86 or `MS` for Microsoft Visual Studio
LIBUSB_COMP=MinGW

# Pass RELEASE=anything to build without debug symbols
ifneq (,$(strip $(RELEASE)))
	BASE_CFLAGS := -O1
else
	BASE_CFLAGS := -g -O0
endif

# Pass LIBUSB_QUIET=anything to Make to silence debug output from libusb.
ifneq (,$(strip $(LIBUSB_QUIET)))
	BASE_CFLAGS += -DSTM8FLASH_LIBUSB_QUIET
endif

BASE_CFLAGS += --std=gnu99 --pedantic

ifeq ($(PLATFORM),Linux)
	LIBS = `pkg-config --libs libusb-1.0`
	LIBUSB_CFLAGS = `pkg-config --cflags libusb-1.0`
else ifeq ($(PLATFORM),Darwin)
	LIBS = $(shell pkg-config --libs libusb-1.0)
	LIBUSB_CFLAGS = $(shell pkg-config --cflags libusb-1.0)
	#MacOSSDK=$(shell xcrun --show-sdk-path)
	#BASE_CFLAGS += -I$(MacOSSDK)/usr/include/ -I$(MacOSSDK)/usr/include/sys -I$(MacOSSDK)/usr/include/machine
else ifeq ($(PLATFORM),FreeBSD)
	LIBS = `pkg-config --libs libusb-1.0`
	LIBUSB_CFLAGS = `pkg-config --cflags libusb-1.0`
else
# 	Generic case is Windows
	
	# Modification Start @boseji <salearj@hotmail.com> 2017-08-04
	BASE_CFLAGS += -I$(LIBUSB_DIR)/include
	LIBS   = -lusb-1.0 -L$(PWD)/$(LIBUSB_DIR)/$(LIBUSB_COMP)$(LIBUSB_ARCH)/static
	# Modification End @boseji <salearj@hotmail.com> 2017-08-04 

	LIBUSB_CFLAGS =
	CC	   = gcc
	BIN_SUFFIX =.exe
endif

# Respect user-supplied cflags, if any - just put ours in front.
override CFLAGS := $(BASE_CFLAGS) $(LIBUSB_CFLAGS) $(CFLAGS)


BIN 		=stm8flash
OBJECTS 	=stlink.o stlinkv2.o main.o byte_utils.o ihex.o srec.o stm8.o


.PHONY: all clean install

$(BIN)$(BIN_SUFFIX): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LIBS) -o $(BIN)$(BIN_SUFFIX)

all: $(BIN)$(BIN_SUFFIX)


clean:
	-rm -f $(OBJECTS) $(BIN)$(BIN_SUFFIX)

install:
ifeq ($(PLATFORM),FreeBSD)
	mkdir -p $(DESTDIR)/usr/local/bin/
	cp $(BIN)$(BIN_SUFFIX) $(DESTDIR)/usr/local/bin/
else
	mkdir -p $(DESTDIR)/usr/bin/
	cp $(BIN)$(BIN_SUFFIX) $(DESTDIR)/usr/bin/
endif
