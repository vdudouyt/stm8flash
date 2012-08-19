/* stlink/v2 device driver
   (c) Valentin Dudouyt, 2012 */

#ifndef __STLINK_H
#define __STLINK_H

#include <stdio.h>
#include <libusb.h>
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

stlink_context_t *stlink_init(unsigned int dev_id);
stlink_status_t stlink_swim_start(stlink_context_t *context);
int stlink_swim_read_range(stlink_context_t *context, char *buffer, unsigned int start, unsigned int length);
int stlink_swim_write_range(stlink_context_t *context, char *buffer, unsigned int start, unsigned int length);

#endif
