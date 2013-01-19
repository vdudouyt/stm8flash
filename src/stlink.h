/* stlink/v2 device driver
   (c) Valentin Dudouyt, 2012 */

#ifndef __STLINK_H
#define __STLINK_H

#include <stdio.h>
#include <libusb.h>
#include <stdbool.h>
#include "stlink.h"

typedef struct stlink_context_s {
	libusb_device_handle *dev_handle;
	libusb_context *ctx;
	unsigned int debug;
} stlink_context_t;

typedef enum {
	STLK_OK = 0,
	STLK_USB_ERROR,
	STLK_SWIM_ERROR
} stlink_status_t;

bool stlink_open(programmer_t *pgm);
bool stlink2_open(programmer_t *pgm);
void stlink_close(programmer_t *pgm);
stlink_status_t stlink_swim_start(stlink_context_t *context);
int stlink2_swim_read_range(programmer_t *pgm, char *buffer, unsigned int start, unsigned int length);
int stlink2_swim_write_range(programmer_t *pgm, char *buffer, unsigned int start, unsigned int length);

#endif
