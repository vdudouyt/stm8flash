# stm8flash makefile
#
# Multiplatform support
#   - Linux (x86, Raspian)
#   - MacOS (Darwin)
#   - Windows (e.g. mingw-w64-x86 with libusb1.0)

GIT_VERSION=$(firstword $(subst ., ,$(shell git log -1 --format="%<(10,trunc)%H")))$(shell git diff --quiet || echo '-dirty')

PLATFORM=$(shell uname -s)

DEFINES+=-DGIT_VERSION=$(GIT_VERSION)

BIN=bin
SRC=src
INSTALLDIR=/usr/local/bin
SRCS=
SRCS+=stlinkv2.c
SRCS+=binary.c
SRCS+=ihex.c
SRCS+=main.c
SRCS+=srec.c
SRCS+=stm8.c
SRCS+=region.c
SRCS+=parts-db.c

PROG=stm8flash
OBJS=$(addprefix $(BIN)/, $(addsuffix .o, $(SRCS)))
OUTPUT=$(BIN)/$(PROG)$(PROG_SUFFIX)

# Pass LIBUSB_QUIET=anything to Make to silence debug output from libusb.
ifneq (,$(strip $(LIBUSB_QUIET)))
	BASE_CFLAGS += -DSTM8FLASH_LIBUSB_QUIET
endif

ifeq ($(PLATFORM),Linux)
	LIBS=`pkg-config --libs libusb-1.0`
	LIBUSB_CFLAGS=`pkg-config --cflags libusb-1.0`
	PROG_SUFFIX=
else ifeq ($(PLATFORM),Darwin)
	LIBS=$(shell pkg-config --libs libusb-1.0)
	LIBUSB_CFLAGS=$(shell pkg-config --cflags libusb-1.0)
	#MacOSSDK=$(shell xcrun --show-sdk-path)
	#BASE_CFLAGS += -I$(MacOSSDK)/usr/include/ -I$(MacOSSDK)/usr/include/sys -I$(MacOSSDK)/usr/include/machine
	PROG_SUFFIX=
else ifeq ($(PLATFORM),FreeBSD)
	LIBS=`pkg-config --libs libusb-1.0`
	LIBUSB_CFLAGS=`pkg-config --cflags libusb-1.0`
	PROG_SUFFIX=
else
	# Generic case is Windows
	DEFINES+=-D_WIN32
	LIBS= -lusb-1.0
	LIBUSB_CFLAGS=
	CC	   ?= gcc
	PROG_SUFFIX=.exe
endif

.PHONY: all release debug clean install

bin/%.c.o:src/%.c
	@mkdir -p $(dir $@)
	$(CC) --std=gnu99 -Wall -O1 $(DEFINES) -c $< -o $@ $(LIBUSB_CFLAGS)

all: $(OUTPUT)
	@echo SUCCESS $(GIT_VERSION)

$(OUTPUT): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

$(OBJS): $(wildcard $(SRC)/*.h)

clean:
	rm -rf $(BIN)

install:
	mkdir -p $(INSTALLDIR)
	cp $(OUTPUT) $(INSTALLDIR)

