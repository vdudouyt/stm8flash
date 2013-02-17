/* stlink common and stlink-v1 specific functions
   (c) Valentin Dudouyt, 2012-2013 */

#include <stdio.h>
#include <libusb.h>
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <endian.h>
#include "pgm.h"
#include "stlink.h"

void stlink_send_message(programmer_t *pgm, int count, ...) {
	va_list ap;
	char data[32];
	int i, r, actual;

	va_start(ap, count);
	assert(pgm->out_msg_size < sizeof(data));
	assert(count <= pgm->out_msg_size);
	memset(data, 0, pgm->out_msg_size);
	for(i = 0; i < count; i++)
		data[i] = va_arg(ap, int);
	r = libusb_bulk_transfer(pgm->dev_handle, (2 | LIBUSB_ENDPOINT_OUT), data, pgm->out_msg_size, &actual, 0);
	if(pgm->out_usleep)
		usleep(pgm->out_usleep);
	if(pgm->debug)
		fprintf(stderr, "msg_count=%d r=%d actual=%d\n", pgm->msg_count, r, actual);
	assert(r == 0);
	pgm->msg_count++;
	return;
}

int stlink_read(programmer_t *pgm, char *buffer, int count) {
	int r, recv;
	r = libusb_bulk_transfer(pgm->dev_handle, (1 | LIBUSB_ENDPOINT_IN), buffer, count, &recv, 0);
	assert(r==0);
	if(pgm->debug && recv > 16) printf("Received block recv=%d\n", recv);
	return(recv);
}

int stlink_read1(programmer_t *pgm, int count) {
	char buf[16];
	int recv = stlink_read(pgm, buf, count);
	if(pgm->debug) {
		// Dumping received data
		int i;
		for(i = 0; i < recv; i++)
			fprintf(stderr, "%02x", buf[i]);
		fprintf(stderr, "\n");
	}
	return(recv);
}

int stlink_read_and_cmp(programmer_t *pgm, int count, ...) {
	va_list ap;
	char buf[16];
	int recv = stlink_read(pgm, buf, count);
	int i, ret = 0;
	va_start(ap, count);
	for(i = 0; i < count; i++) {
		if(buf[i] != va_arg(ap, int))
			ret++;
	}
	return(ret);
}

char *pack_int16(uint16_t word, char *out) {
	// Filling with bytes in big-endian order
	out[0] = (word & 0xff00) >> 8;
	out[1] = (word & 0x00ff);
	return(out+2);
}

uint16_t unpack_int16_le(char *block) {
	uint32_t ret;
	ret = *(block + 1) << 8;
	ret += *(block + 0);
	return(ret);
}

char *pack_int32(uint32_t word, char *out) {
	out[0] = (word & 0xff000000) >> 24;
	out[1] = (word & 0x00ff0000) >> 16;
	out[2] = (word & 0x0000ff00) >> 8;
	out[3] = (word & 0x000000ff);
	return(out+4);
}

char *pack_int32_le(uint32_t word, char *out) {
	// Filling with bytes in little-endian order
	out[0] = (word & 0x000000ff);
	out[1] = (word & 0x0000ff00) >> 8;
	out[2] = (word & 0x00ff0000) >> 16;
	out[3] = (word & 0xff000000) >> 24;
	return(out+4);
}

uint32_t unpack_int32(char *block) {
	uint32_t ret;
	ret  = *(block + 0) << 24;
	ret += *(block + 1) << 16;
	ret += *(block + 2) << 8;
	ret += *(block + 3);
	return(ret);
}

uint32_t unpack_int32_le(char *block) {
	uint32_t ret;
	ret  = *(block + 3) << 24;
	ret += *(block + 2) << 16;
	ret += *(block + 1) << 8;
	ret += *(block + 0);
	return(ret);
}

void pack_usb_cbw(scsi_usb_cbw *cbw, char *out) {
	char *offset = out;
	offset = pack_int32(cbw->signature, offset);
	offset = pack_int32(cbw->tag, offset);
	offset = pack_int32_le(cbw->transfer_length, offset);
	offset[0] = cbw->flags;
	offset[1] = cbw->LUN;
	offset[2] = cbw->cblength;
	offset += 3;
	memcpy(offset, cbw->cb, sizeof(cbw->cb));
	offset += sizeof(cbw->cb);
	assert(offset - out == USB_CBW_SIZE);
}

void unpack_usb_csw(char *block, scsi_usb_csw *out) {
	out->signature = unpack_int32(block);
	out->tag = unpack_int32(block + 4);
	out->data_residue = unpack_int32_le(block + 8);
	out->status = block[12];
}

int stlink_send_cbw(libusb_device_handle *dev_handle, scsi_usb_cbw *cbw) {
	unsigned char buf[USB_CBW_SIZE];
	cbw->signature = USB_CBW_SIGNATURE;
	cbw->tag = 0x707ec281;
	pack_usb_cbw(cbw, buf);
	int actual;
	int r = libusb_bulk_transfer(dev_handle,
				(2 | LIBUSB_ENDPOINT_OUT),
				buf,
				USB_CBW_SIZE,
				&actual,
				0);
	assert(actual == USB_CBW_SIZE);
	return(r);
}

int stlink_read_csw(libusb_device_handle *dev_handle, scsi_usb_csw *csw) {
	unsigned char buf[USB_CSW_SIZE];
	int recv;
	int r = libusb_bulk_transfer(dev_handle,
				(1 | LIBUSB_ENDPOINT_IN),
				buf,
				USB_CSW_SIZE,
				&recv,
				0);
	assert(recv == USB_CSW_SIZE);
	unpack_usb_csw(buf, csw);
	return(r);
}

scsi_usb_cbw cbw;
scsi_usb_csw csw;

int stlink_test_unit_ready(programmer_t *pgm) {
	// This is a default SCSI command
	memset(&cbw, 0, sizeof(scsi_usb_cbw));
	cbw.cblength = 0x06;
	int r;
	r = stlink_send_cbw(pgm->dev_handle, &cbw);
	assert(r == 0);
	r = stlink_read_csw(pgm->dev_handle, &csw);
	assert(r == 0);
	return(csw.status == 0);
}

int stlink_cmd(programmer_t *pgm, int transfer_length, unsigned char *transfer_out, unsigned char flags,
			int cblength, ...) {
	va_list ap;
	memset(&cbw, 0, sizeof(scsi_usb_cbw));
	cbw.transfer_length = transfer_length;
	cbw.flags = flags;
	cbw.cblength = cblength;
	va_start(ap, cblength);
	int i;
	for(i = 0; i < cblength; i++) {
		cbw.cb[i] = va_arg(ap, int);
	}
	assert( stlink_send_cbw(pgm->dev_handle, &cbw) == 0);
	if(transfer_length) {
		// Transfer expected, read some raw data
		if(transfer_out)
			stlink_read(pgm, transfer_out, transfer_length);
		else
			stlink_read1(pgm, transfer_length);
	}
	// Reading status
	stlink_read_csw(pgm->dev_handle, &csw);
	return(csw.status == 0);
}

int stlink_cmd_swim_read(programmer_t *pgm, uint16_t length, uint16_t start) {
	// This command isn't found in SCSI opcodes specification.
	memset(&cbw, 0, sizeof(scsi_usb_cbw));
	cbw.transfer_length = length;
	cbw.flags = 0x80;
	cbw.cblength = 0x0a;
	cbw.cb[0] = 0xf4;
	cbw.cb[1] = 0x0c;
	pack_int16(length, cbw.cb+2);
	pack_int16(start, cbw.cb+6);
}

bool stlink_open(programmer_t *pgm) {
	char buf[18];
	pgm->out_msg_size = 31;
	stlink_test_unit_ready(pgm);
	stlink_cmd(pgm, 0x06, buf, 0x80, 6, 0xf1, 0x80, 0x00, 0x00, 0x00, 0x00);
	stlink_test_unit_ready(pgm);
	stlink_cmd(pgm, 0x12, buf, 0x80, 6, 0x12, 0x80, 0x00, 0x00, 0x20);
	stlink_test_unit_ready(pgm);
	stlink_cmd(pgm, 2, buf, 0x80, 2, 0xf5, 0x00); // Reading status
	stlink_test_unit_ready(pgm);
	int status = unpack_int16_le(buf);
	switch(status) {
		case 0x0000: // Ok
			stlink_cmd(pgm, 0, NULL, 0x80, 2, 0xf3, 0x07); // Start initializing sequence
			stlink_cmd(pgm, 0, NULL, 0x00, 2, 0xf4, 0x00); // Turn the lights on
			stlink_cmd(pgm, 2, buf, 0x80, 2, 0xf4, 0x0d);
			stlink_cmd(pgm, 8, buf, 0x80, 3, 0xf4, 0x02, 0x01); // End init
		case 0x0003: // Already initialized
			return(true);
		case 0x0001: // Busy
			break;
		default:
			fprintf(stderr, "Unknown status: %x\n", status);
	}
	return(false);
}

void stlink_close(programmer_t *pgm) {
	libusb_exit(pgm->ctx); //close the session
}

int stlink_swim_read_range(programmer_t *pgm, char *buffer, unsigned int start, unsigned int length) {
	printf("stlink_swim_read_range\n");
}
int stlink_swim_write_range(programmer_t *pgm, char *buffer, unsigned int start, unsigned int length) {
	printf("stlink_swim_write_range\n");
}
