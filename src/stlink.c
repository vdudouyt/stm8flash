/* stlink common and stlink-v1 specific functions
   (c) Valentin Dudouyt, 2012-2013 */

#include <stdio.h>
#include <libusb.h>
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include "pgm.h"
#include "stlink.h"

void stlink_send_message(programmer_t *pgm, int count, ...) {
	va_list ap;
	char data[16];
	int i, r, actual;

	va_start(ap, count);
	memset(data, 0, 16);
	for(i = 0; i < count; i++)
		data[i] = va_arg(ap, int);
	r = libusb_bulk_transfer(pgm->dev_handle, (2 | LIBUSB_ENDPOINT_OUT), data, sizeof(data), &actual, 0);
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

bool stlink_open(programmer_t *pgm) {
	printf("Not implemented yet. Will get a SWIM error message.\n");
	int i;
	for(i = 0; i < 15; i++) {
		//stlink_send_message(pgm, 5553424308f0cc81
	}
}
void stlink_close(programmer_t *pgm) {
	libusb_exit(pgm->ctx); //close the session
}
