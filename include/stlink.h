/* stlink/v2 device driver
   (c) Valentin Dudouyt, 2012 */

#ifndef __STLINK_H
#define __STLINK_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "pgm.h"

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

/* stlinkv1 uses the USB-TO-ATA protocol to communicate (same as your
   USB flash drive), with exception that some additional commands are 
   supported */
#define USB_CBW_SIGNATURE 0x55534243
#define USB_CBW_SIZE 31
typedef struct _scsi_usb_cbw {
	// Command block warper
	uint32_t signature; // Always 0x55534243
	uint32_t tag; // Arbitrary value
	uint32_t transfer_length;
	unsigned char flags;
	unsigned char LUN; // SCSI drive identificator (always 0 here)
	unsigned char cblength;
	unsigned char cb[16]; // Raw command
} scsi_usb_cbw;

#define USB_CSW_SIZE 13
typedef struct _scsi_usb_csw {
	// Command status warper
	uint32_t signature; // Always 0x55534243
	uint32_t tag; // Same as passed with CBW
	uint32_t data_residue; // cbw.transfer_length - actual handled
	unsigned char status;
} scsi_usb_csw;

bool stlink_open(programmer_t *pgm);
void stlink_close(programmer_t *pgm);
void stlink_swim_srst(programmer_t *pgm);
int stlink_swim_read_range(programmer_t *pgm, const stm8_device_t *device, unsigned char *buffer, unsigned int start, unsigned int length);
int stlink_swim_write_range(programmer_t *pgm, const stm8_device_t *device, unsigned char *buffer, unsigned int start, unsigned int length, const memtype_t memtype);

#endif
