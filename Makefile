.PHONY: all clean
OBJECTS=stlink.o stlinkv2.o main.o byte_utils.o ihex.o stm8.o
CFLAGS = `pkg-config --cflags libusb-1.0` -g -O0
LIBS = `pkg-config --libs libusb-1.0`
BIN = stm8flash

all: $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o $(BIN)

clean:
	-rm -f $(OBJECTS) $(BIN)

install:
	cp $(BIN) $(DESTDIR)/usr/bin/
