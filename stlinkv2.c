/* stlink-v2 specific functions
   (c) Valentin Dudouyt, 2012-2013 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include "pgm.h"
#include "stlink.h"
#include "error.h"
#include "try.h"
#include "byte_utils.h"
#include "stlinkv2.h"

#define STLK_READ_BUFFER_SIZE 6144

char cmd_buf[16];

unsigned int stlink2_get_status(programmer_t *pgm);
int stlink2_write_byte(programmer_t *pgm, unsigned char byte, unsigned int start);
int stlink2_write_and_read_byte(programmer_t *pgm, unsigned char byte, unsigned int start);

static unsigned int msg_transfer(programmer_t *pgm, char *buf, unsigned int length, int direction) {
	int bytes_transferred;
	int ep = (direction == LIBUSB_ENDPOINT_OUT) ? 2 : 1;
	libusb_bulk_transfer(pgm->dev_handle, ep | direction, buf, length, &bytes_transferred, 0);
	if(bytes_transferred != length) ERROR2("IO error: expected %d bytes but %d bytes transferred\n", length, bytes_transferred);
	return bytes_transferred;
}

static unsigned int msg_send(programmer_t *pgm, char *buf, unsigned int length) {
	return msg_transfer(pgm, buf, length, LIBUSB_ENDPOINT_OUT);
}

static unsigned int msg_recv(programmer_t *pgm, char *buf, unsigned int length) {
	return msg_transfer(pgm, buf, length, LIBUSB_ENDPOINT_IN);
}

static void msg_recv0(programmer_t *pgm, unsigned int length) {
	char buf[64];
	msg_recv(pgm, buf, length);
}

static unsigned int msg_recv_int(programmer_t *pgm, unsigned int length) {
	char buf[4];
	msg_recv(pgm, buf, length);
	return load_int(buf, length, MP_LITTLE_ENDIAN);
}

static unsigned int msg_recv_int8(programmer_t *pgm) {	return msg_recv_int(pgm, 1); }
static unsigned int msg_recv_int16(programmer_t *pgm) {	return msg_recv_int(pgm, 2); }
static unsigned int msg_recv_int32(programmer_t *pgm) {	return msg_recv_int(pgm, 4); }

static void msg_init(char *out, unsigned int cmd) {
	memset(out, 0, 16);
	format_int(out, cmd, 2, MP_BIG_ENDIAN);
}

static void stlink2_cmd(programmer_t *pgm, unsigned int cmd, unsigned int length, ...) {
	va_list ap;
	int i;

	// Preparing
	msg_init(cmd_buf, cmd);
	va_start(ap, length);
	for(i = 0; i < length; i++) {
		cmd_buf[i + 2] = va_arg(ap, int);
	}
	va_end(ap);

	// Triggering USB transfer
	msg_send(pgm, cmd_buf, sizeof(cmd_buf));
}

bool stlink2_open(programmer_t *pgm) {
	stlink2_cmd(pgm, 0xf500, 0);
	switch(msg_recv_int16(pgm)) {
		case 0x0100:
		case 0x0001:
			// Run initializing sequence
			stlink2_cmd(pgm, 0xf307, 0); // Start initializing sequence
			stlink2_cmd(pgm, 0xf400, 0); // Turns the lights on
		case 0x0003:
			stlink2_cmd(pgm, 0xf40d, 0);
			msg_recv_int16(pgm);
			stlink2_cmd(pgm, 0xf402, 1, 0x01);
			msg_recv0(pgm, 8);
			break;
	}
	return(true);
}

void stlink2_srst(programmer_t *pgm) {
	stlink2_cmd(pgm, 0xf407, 2, 0x00, 0x01);
	stlink2_get_status(pgm);
	stlink2_cmd(pgm, 0xf408, 2, 0x00, 0x01);
	stlink2_get_status(pgm);
}

void stlink2_init_session(programmer_t *pgm) {
	int i;
	char f4_cmd_arg1[] = {	0x07,
				0x07,
				0x08,
				0x07,
				0x04,
				0x03,
				0x05,
				};
	for(i = 0; i < sizeof(f4_cmd_arg1); i++) {
		stlink2_cmd(pgm, 0xf400 | f4_cmd_arg1[i], 0);
		TRY(8, stlink2_get_status(pgm) == 0);
	}

	stlink2_write_byte(pgm, 0xa0, 0x7f80); // mov 0x0a, SWIM_CSR2 ;; Init SWIM
	stlink2_cmd(pgm, 0xf408, 0);
	TRY(8, stlink2_get_status(pgm) == 0);

	stlink2_write_and_read_byte(pgm, 0xa0, 0x7f99);
	stlink2_cmd(pgm, 0xf40c, 0);
	msg_recv_int8(pgm); // 0x08 (or 0x0a if used stlink2_write_byte() instead)
}

void stlink2_finish_session(programmer_t *pgm) {
	stlink2_cmd(pgm, 0xf405, 0);
	stlink2_get_status(pgm);
	stlink2_cmd(pgm, 0xf407, 0);
	stlink2_get_status(pgm);
	stlink2_cmd(pgm, 0xf403, 0);
	stlink2_get_status(pgm);
}

int stlink2_write_byte(programmer_t *pgm, unsigned char byte, unsigned int start) {
	char buf[4], start2[2];
	pack_int16(start, start2);
	stlink2_cmd(pgm, 0xf40a, 7,
			0x00, 0x01,
			0x00, 0x00,
			HI(start), LO(start),
			byte);
	usleep(2000);
	return(stlink2_get_status(pgm)); // Should be '1'
}

int stlink2_write_word(programmer_t *pgm, unsigned int word, unsigned int start) {
	char buf[4], start2[2];
	pack_int16(start, start2);
	stlink2_cmd(pgm, 0xf40a, 8,
			0x00, 0x02,
			0x00, 0x00,
			HI(start), LO(start),
			HI(word), LO(word));
	usleep(2000);
	return(stlink2_get_status(pgm)); // Should be '1'
}

int stlink2_write_and_read_byte(programmer_t *pgm, unsigned char byte, unsigned int start) {
	char buf[4], start2[2];
	pack_int16(start, start2);
	stlink2_cmd(pgm, 0xf40b, 7,
			0x00, 0x01,
			0x00, 0x00,
			HI(start), LO(start),
			byte);
	usleep(2000);
	stlink2_get_status(pgm);

	stlink2_cmd(pgm, 0xf40c, 0);
	return(msg_recv_int8(pgm));
}

unsigned int stlink2_get_status(programmer_t *pgm) {
	stlink2_cmd(pgm, 0xf409, 0);
	return msg_recv_int32(pgm);
}

int stlink2_swim_read_range(programmer_t *pgm, stm8_device_t *device, char *buffer, unsigned int start, unsigned int length) {
	stlink2_init_session(pgm);

	int i;
	for(i = 0; i < length; i += STLK_READ_BUFFER_SIZE) {
		// Determining current block start & size (relative to 0x8000)
		int block_start = start + i;
		int block_size = length - i;
		if(block_size > STLK_READ_BUFFER_SIZE) {
			block_size = STLK_READ_BUFFER_SIZE;
		}

		// Sending USB packet
		stlink2_cmd(pgm, 0xf40b, 6,
				HI(block_size), LO(block_size),
				0x00, 0x00, 
				HI(block_start), LO(block_start));
		TRY(128, (stlink2_get_status(pgm) & 0xffff) == 0);

		// Seems like we've got some bytes from stlink, downloading them
		stlink2_cmd(pgm, 0xf40c, 0);
		msg_recv(pgm, &(buffer[i]), block_size);
	}

	return(length);
}

void stlink2_wait_until_transfer_completes(programmer_t *pgm, stm8_device_t *device) {
	TRY(8, stlink2_write_and_read_byte(pgm, 0x82, device->regs.FLASH_IAPSR) & 0x4);
}

int stlink2_swim_write_range(programmer_t *pgm, stm8_device_t *device, char *buffer, unsigned int start, unsigned int length) {
	stlink2_init_session(pgm);

	stlink2_write_byte(pgm, 0x00, device->regs.CLK_CKDIVR);
	stlink2_write_and_read_byte(pgm, 0x00, device->regs.FLASH_IAPSR);

	stlink2_write_byte(pgm, 0x56, device->regs.FLASH_PUKR);
	stlink2_write_byte(pgm, 0xae, device->regs.FLASH_PUKR); 
	stlink2_write_byte(pgm, 0xae, device->regs.FLASH_DUKR);
	stlink2_write_byte(pgm, 0x56, device->regs.FLASH_DUKR);
	stlink2_write_and_read_byte(pgm, 0x56, device->regs.FLASH_IAPSR); // mov 0x56, FLASH_IAPSR

	int i;
	int BLOCK_SIZE = device->flash_block_size;
	for(i = 0; i < length; i+=BLOCK_SIZE) {
		stlink2_write_word(pgm, 0x01fe, device->regs.FLASH_CR2); // mov 0x01fe, FLASH_CR2

		// The first 8 packet bytes are getting transmitted
		// with the same USB bulk transfer as the command itself
		msg_init(cmd_buf, 0xf40a);
		format_int(&(cmd_buf[2]), BLOCK_SIZE, 2, MP_BIG_ENDIAN);
		format_int(&(cmd_buf[6]), start + i, 2, MP_BIG_ENDIAN);
		memcpy(&(cmd_buf[8]), &(buffer[i]), 8);
		msg_send(pgm, cmd_buf, sizeof(cmd_buf));

		// Transmitting the rest
		msg_send(pgm, &(buffer[i + 8]), BLOCK_SIZE - 8);

		// Waiting for the transfer to process
		TRY(128, HI(stlink2_get_status(pgm)) == BLOCK_SIZE);

		stlink2_wait_until_transfer_completes(pgm, device);
	}
	stlink2_write_and_read_byte(pgm, 0x56, device->regs.FLASH_IAPSR); // mov 0x56, FLASH_IAPSR
	stlink2_write_byte(pgm, 0x00, 0x7f80);
	stlink2_write_byte(pgm, 0xb6, 0x7f80);
	stlink2_finish_session(pgm);
	return(length);
}

