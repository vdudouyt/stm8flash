# stm8flash makefile
#
# Multiplatform support
#   - Linux (x86, Raspian)
#   - MacOS (Darwin)
#   - Windows (e.g. mingw-w64-x86 with libusb1.0)


PLATFORM=$(shell uname -s)

ifeq ($(PLATFORM),Linux)
	LIBS = `pkg-config --libs libusb-1.0`
	CFLAGS = `pkg-config --cflags libusb-1.0` -g -O0 --std=gnu99 --pedantic
else ifeq ($(PLATFORM),Darwin)
	LIBS = `pkg-config --libs libusb-1.0`
	CFLAGS = `pkg-config --cflags libusb-1.0` -g -O0 --std=gnu99 --pedantic
  	MacOSSDK=`xcrun --show-sdk-path`
  	CFLAGS += -I$(MacOSSDK)/usr/include/ -I$(MacOSSDK)/usr/include/sys -I$(MacOSSDK)/usr/include/machine
else 
# 	Generic case is Windows

	LIBS   = -lusb-1.0
	CFLAGS = -g -O0 --std=gnu99 --pedantic 
	CC	   = GCC
	BIN_SUFFIX =.exe
endif

BIN 		=stm8flash
OBJECTS 	=stlink.o stlinkv2.o main.o byte_utils.o ihex.o stm8.o


.PHONY: all clean

all: $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o $(BIN)

clean:
	-rm -f $(OBJECTS) $(BIN)$(BIN_SUFFIX)

install:
	cp $(BIN)$(BIN_SUFFIX) $(DESTDIR)/usr/bin/
