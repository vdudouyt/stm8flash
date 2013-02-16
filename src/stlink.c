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

char *pack_int32(uint32_t word, char *out) {
	// Filling with bytes in big-endian order
	out[0] = (word & 0xff000000) >> 24;
	out[1] = (word & 0x00ff0000) >> 16;
	out[2] = (word & 0x0000ff00) >> 8;
	out[3] = (word & 0x000000ff);
	// Skipping them
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

void pack_usb_cbw(scsi_usb_cbw *cbw, char *out) {
	char *offset = out;
	offset = pack_int32(cbw->signature, offset);
	offset = pack_int32(cbw->tag, offset);
	offset = pack_int32(cbw->transfer_length, offset);
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
	out->data_residue = unpack_int32(block + 8);
	out->status = block[12];
}

int stlink_send_cbw(libusb_device_handle *dev_handle, scsi_usb_cbw *cbw) {
	unsigned char buf[USB_CBW_SIZE];
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

int stlink_test_unit_ready(programmer_t *pgm) {
	scsi_usb_cbw cbw;
	scsi_usb_csw csw;
	memset(&cbw, 0, sizeof(scsi_usb_cbw));
	cbw.signature = USB_CBW_SIGNATURE;
	cbw.tag = 0x707ec281;
	cbw.cblength = 0x06;
	int r;
	r = stlink_send_cbw(pgm->dev_handle, &cbw);
	assert(r == 0);
	r = stlink_read_csw(pgm->dev_handle, &csw);
	assert(r == 0);
	return(csw.status == 0);
}

bool stlink_open(programmer_t *pgm) {
	pgm->out_msg_size = 31;
	int r = stlink_test_unit_ready(pgm);
	printf("success: %d\n", r);
	exit(-1);
//	int i;
//	for(i = 0; i < 15; i++) {
//		stlink_send_message(pgm, 15, 0x55, 0x53, 0x42, 0x43, 0x08, 0xf0, 0xcc, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06);
//		stlink_read1(pgm, 13);
//		sleep(1);
//	}
	return(true);
}

void stlink_close(programmer_t *pgm) {
	libusb_exit(pgm->ctx); //close the session
}

int stlink_swim_read_range(programmer_t *pgm, char *buffer, unsigned int start, unsigned int length) {
}
int stlink_swim_write_range(programmer_t *pgm, char *buffer, unsigned int start, unsigned int length) {
}
