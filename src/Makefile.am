bin_PROGRAMS = stm8flash
stm8flash_SOURCES = stlink.c stlinkv2.c espstlink.c main.c byte_utils.c ihex.c srec.c stm8.c
LDADD = $(LIBUSB_LINK)
AM_CFLAGS = -I$(top_srcdir)/include $(LIBUSB_INCLUDE) 
