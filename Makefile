.PHONY: all clean
OBJECTS=stlink.o stlinkv2.o main.o byte_utils.o ihex.o stm8.o
CFLAGS = `pkg-config --cflags libusb-1.0` -g -O0 --std=gnu99 --pedantic
PLATFORM=$(shell uname -s)
ifeq ($(PLATFORM),Darwin)
  MacOSSDK=`xcrun --show-sdk-path`
  CFLAGS += -I$(MacOSSDK)/usr/include/ -I$(MacOSSDK)/usr/include/sys -I$(MacOSSDK)/usr/include/machine
endif
LIBS = `pkg-config --libs libusb-1.0`
BIN = stm8flash

all: $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o $(BIN)

clean:
	-rm -f $(OBJECTS) $(BIN)

install:
	mkdir -p $(DESTDIR)/usr/bin/
	cp $(BIN) $(DESTDIR)/usr/bin/
