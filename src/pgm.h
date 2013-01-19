#ifndef __PGM_H
#define __PGM_H

#include <libusb.h>

typedef struct programmer_s {
	/* Info */
	const char *name;
	unsigned int usb_vid;
	unsigned int usb_pid;

	/* Methods */
	bool (*open) (struct programmer_s *pgm);
	void (*close) (struct programmer_s *pgm);
	int (*read_range) (struct programmer_s *pgm, char *buffer, unsigned int start, unsigned int length);
	int (*write_range) (struct programmer_s *pgm, char *buffer, unsigned int start, unsigned int length);

	/* Private */
	libusb_device_handle *dev_handle;
	libusb_context *ctx;
	unsigned int debug;
	unsigned int msg_count; // debugging only
	unsigned int out_usleep; // stlinkv2 implementation specific, will be deprecated
} programmer_t;

typedef bool (*pgm_open_cb)(programmer_t *);
typedef void (*pgm_close_cb)(programmer_t *);
typedef int (*pgm_read_range_cb)(programmer_t *, char *, unsigned int, unsigned int);
typedef int (*pgm_write_range_cb)(programmer_t *, char *, unsigned int, unsigned int);

#endif
