/* stlink common and stlink-v1 specific functions
   (c) Valentin Dudouyt, 2012-2013 */

#include <stdio.h>
#include <stddef.h>

#include <malloc.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include "stm8.h"
#include "pgm.h"
#include "stlink.h"
#include "utils.h"

#ifndef WIN32
#include <endian.h>
#endif

#define STLK_FLAG_ERR 0x01
#define STLK_FLAG_BUFFER_FULL 0x04
#define STLK_READ_BUFFER_SIZE 6144
#define STLK_MAX_WRITE 512

unsigned int stlink_swim_get_status(programmer_t *pgm);
int stlink_swim_read_byte(programmer_t *pgm, unsigned char byte, unsigned int start);
int stlink_swim_write_byte(programmer_t *pgm, unsigned char byte, unsigned int start);

void stlink_send_message(programmer_t *pgm, int count, ...) {
	va_list ap;
	unsigned char data[32];
	int i, r, actual;

	va_start(ap, count);
	assert(pgm->out_msg_size < sizeof(data));
	assert(count <= pgm->out_msg_size);
	memset(data, 0, pgm->out_msg_size);
	for(i = 0; i < count; i++)
		data[i] = va_arg(ap, int);
	r = libusb_bulk_transfer(pgm->dev_handle, (2 | LIBUSB_ENDPOINT_OUT), data, pgm->out_msg_size, &actual, 0);
	assert(r == 0);
	pgm->msg_count++;
	return;
}

int stlink_read(programmer_t *pgm, unsigned char *buffer, int count) {
	int r, recv;
	r = libusb_bulk_transfer(pgm->dev_handle, (1 | LIBUSB_ENDPOINT_IN), buffer, count, &recv, 0);
	assert(r==0);
	return(recv);
}

int stlink_read1(programmer_t *pgm, int count) {
	unsigned char buf[16];
	return(stlink_read(pgm, buf, count));
}

int stlink_read_and_cmp(programmer_t *pgm, int count, ...) {
	va_list ap;
	unsigned char buf[16];
	int recv = stlink_read(pgm, buf, count);
	int i, ret = 0;
	va_start(ap, count);
	for(i = 0; i < count; i++) {
		if(buf[i] != va_arg(ap, int))
			ret++;
	}
	return(ret);
}

unsigned char *pack_int16(uint16_t word, unsigned char *out) {
	// Filling with bytes in big-endian order
	out[0] = (word & 0xff00) >> 8;
	out[1] = (word & 0x00ff);
	return(out+2);
}

uint16_t unpack_int16_le(unsigned char *block) {
	uint32_t ret;
	ret = *(block + 1) << 8;
	ret += *(block + 0);
	return(ret);
}

uint16_t unpack_int16(unsigned char *block) {
	uint32_t ret;
	ret = *(block + 0) << 8;
	ret += *(block + 1);
	return(ret);
}

unsigned char *pack_int32(uint32_t word, unsigned char *out) {
	out[0] = (word & 0xff000000) >> 24;
	out[1] = (word & 0x00ff0000) >> 16;
	out[2] = (word & 0x0000ff00) >> 8;
	out[3] = (word & 0x000000ff);
	return(out+4);
}

unsigned char *pack_int32_le(uint32_t word, unsigned char *out) {
	// Filling with bytes in little-endian order
	out[0] = (word & 0x000000ff);
	out[1] = (word & 0x0000ff00) >> 8;
	out[2] = (word & 0x00ff0000) >> 16;
	out[3] = (word & 0xff000000) >> 24;
	return(out+4);
}

uint32_t unpack_int32(unsigned char *block) {
	uint32_t ret;
	ret  = *(block + 0) << 24;
	ret += *(block + 1) << 16;
	ret += *(block + 2) << 8;
	ret += *(block + 3);
	return(ret);
}

uint32_t unpack_int32_le(unsigned char *block) {
	uint32_t ret;
	ret  = *(block + 3) << 24;
	ret += *(block + 2) << 16;
	ret += *(block + 1) << 8;
	ret += *(block + 0);
	return(ret);
}

void pack_usb_cbw(scsi_usb_cbw *cbw, unsigned char *out) {
	unsigned char *offset = out;
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

void unpack_usb_csw(unsigned char *block, scsi_usb_csw *out) {
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
	memset(&cbw, 0, sizeof(scsi_usb_cbw));
	cbw.transfer_length = length;
	cbw.flags = 0x80;
	cbw.cblength = 0x0a;
	cbw.cb[0] = 0xf4;
	cbw.cb[1] = 0x0c;
	pack_int16(length, cbw.cb+2);
	pack_int16(start, cbw.cb+6);
	return 0;
}

void stlink_init_session(programmer_t *pgm) {
	int i;
	char f4_cmd_arg1[] = {	0x07,
				0x07,
				0x08,
				0x07,
				0x04,
				};
	for(i = 0; i < sizeof(f4_cmd_arg1); i++) {
		stlink_cmd(pgm, 0, NULL, 0x00, 0x0a,
				0xf4, f4_cmd_arg1[i],
				0x01, 0x00,
				0x00, 0x00,
				0x00, 0x00,
				0x00, 0x00);
		stlink_swim_get_status(pgm);
	}
	do {
		usleep(10000);
	} while ((stlink_swim_get_status(pgm) & 1) != 0);
	stlink_cmd(pgm, 0, NULL, 0x00, 0x03,
			0xf4, 0x03,
			0x00, 0x00,
			0x00, 0x00,
			0x00, 0x00,
			0x00, 0x00);
	stlink_swim_get_status(pgm);
	stlink_cmd(pgm, 0, NULL, 0x00, 0x0a,
			0xf4, 0x05,
			0x00, 0x00,
			0x00, 0x00,
			0x00, 0x00,
			0x00, 0x00);
	stlink_swim_get_status(pgm);

	stlink_swim_write_byte(pgm, 0xa0, 0x7f80); // mov 0x0a, SWIM_CSR2 ;; Init SWIM
	stlink_cmd(pgm, 0, NULL, 0x00, 0x0a,
			0xf4, 0x08,
			0x00, 0x01,
			0x00, 0x00,
			0x7f, 0x80,
			0xa0, 0x00);
	stlink_swim_get_status(pgm);
	stlink_cmd(pgm, 0, NULL, 0x00, 0x0a,
			0xf4, 0x06,
			0x00, 0x01,
			0x00, 0x00,
			0x7f, 0x99,
			0xa0, 0x00);
	stlink_swim_get_status(pgm);
	stlink_swim_write_byte(pgm, 0xb0, 0x7f80); 
	stlink_cmd(pgm, 0, NULL, 0x00, 0x03, // Elsewise, only zeroes will be received
			0xf4, 0x03,
			0x01);
	stlink_swim_get_status(pgm);
	stlink_swim_write_byte(pgm, 0xb4, 0x7f80); 
}

void stlink_finish_session(programmer_t *pgm) {
	stlink_swim_write_byte(pgm, 0xb6, 0x7f80);
	stlink_cmd(pgm, 0, NULL, 0x00, 0x0a,
			0xf4, 0x05,
			0x00, 0x01,
			0x00, 0x00,
			0x7f, 0x80,
			0xb6, 0x00);
	stlink_swim_get_status(pgm);
	stlink_cmd(pgm, 0, NULL, 0x00, 0x0a,
			0xf4, 0x07,
			0x00, 0x01,
			0x00, 0x00,
			0x7f, 0x80,
			0xb6, 0x00);
	stlink_swim_get_status(pgm);
	stlink_cmd(pgm, 0, NULL, 0x00, 0x03,
			0xf4, 0x03,
			0x01);
	stlink_swim_get_status(pgm);
}

unsigned int stlink_swim_get_status(programmer_t *pgm) {
	unsigned char buf[4];
	stlink_cmd(pgm, 4, buf, 0x80, 0x0a,
			0xf4, 0x09,
			0x01, 0x00,
			0x00, 0x00,
			0x00, 0x00,
			0x00, 0x00);
	return(unpack_int32_le(buf));
}

bool stlink_open(programmer_t *pgm) {
	unsigned char buf[18];
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

void stlink_swim_srst(programmer_t *pgm) {
	// Ready bytes count (always 1 here)
	stlink_cmd(pgm, 0, NULL, 0x80, 0x0a,
			0xf4, 0x08, 
			0x00, 0x01,
			0x00, 0x00, 
			0x00, 0x00, 
			0x00, 0x00);
	stlink_swim_get_status(pgm);
}

int stlink_swim_write_byte(programmer_t *pgm, unsigned char byte, unsigned int start) {
	unsigned char buf[4], start2[2];
	int result, tries = 0;
	pack_int16(start, start2);
	DEBUG_PRINT("stlink_swim_write_byte\n");
	do {
		stlink_cmd(pgm, 0, NULL, 0x00, 0x10,
				0xf4, 0x0a, 
				0x00, 0x01,
				0x00, 0x00,
				start2[0], start2[1],
				byte, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		usleep(2000);
		// Ready bytes count (always 1 here)
		stlink_cmd(pgm, 4, buf, 0x80, 0x0a,
				0xf4, 0x09, 
				0x00, 0x01,
				0x00, 0x00, 
				start2[0], start2[1],
				byte, 0x00);
		result = unpack_int16_le(buf);
		tries++;
		if(result & STLK_FLAG_BUFFER_FULL) {
			usleep(4000); // Chill out
			DEBUG_PRINT("retry\n");
			continue;
		}
		if(result & STLK_FLAG_ERR)
			break;
	} while(result & STLK_FLAG_ERR && tries < 5);
	return(result);
}

int stlink_swim_read_range(programmer_t *pgm, const stm8_device_t *device, unsigned char *buffer, unsigned int start, unsigned int length) {
	unsigned char buf[4];
	DEBUG_PRINT("stlink_swim_read_range\n");
	stlink_init_session(pgm);
	stlink_swim_write_byte(pgm, 0x00, device->regs.CLK_CKDIVR); // mov 0x00, CLK_DIVR
	int i;
	for(i = 0; i < length; i += STLK_READ_BUFFER_SIZE) {
		unsigned char block_start2[2], block_size2[2];
		int block_start = start + i;
		// Determining block size
		int block_size = length - i;
		if(block_size > STLK_READ_BUFFER_SIZE)
			block_size = STLK_READ_BUFFER_SIZE;
		DEBUG_PRINT("Reading %d bytes from %x\n", block_size, block_start);
		// Starting SWIM transfer
		pack_int16(block_start, block_start2);
		pack_int16(block_size, block_size2);
		stlink_cmd(pgm, 0, NULL, 0x80, 0x0a,
				0xf4, 0x0b, 
				block_size2[0], block_size2[1],
				0x00, 0x00, 
				block_start2[0], block_start2[1],
				0x00, 0x00);
		// Waiting until the data becomes ready
		int result;
		do {
			usleep(2000);
			result = stlink_swim_get_status(pgm);
		} while(result & 1);
		// Downloading bytes from stlink
		stlink_cmd(pgm, block_size, &(buffer[i]), 0x80, 0x0a,
				0xf4, 0x0c, 
				block_size2[0], block_size2[1],
				0x00, 0x00, 
				block_start2[0], block_start2[1],
				0x00, 0x00);
	}
	stlink_finish_session(pgm);
	return(length);
}

int stlink_swim_wait(programmer_t *pgm) {
	int result;
	do {
		usleep(3000);
		result = stlink_swim_get_status(pgm);
	} while(result & 1);
	return(result);
}

int stlink_swim_write_block(programmer_t *pgm, unsigned char *buffer,
			unsigned int start,
			unsigned int length,
			unsigned int padding
			) {
	int length1 = 8 - padding; // Amount to be transferred with CBW
	int length2 = length - 8 + padding; // Amount to be transferred with additional transfer
	if (length2 < 0) length2 = 0;
	unsigned char block_size2[2], block_start2[2];
	pack_int16(start, block_start2);
	pack_int16(length, block_size2);
	// Some logical checks
	assert(padding >= 0 && padding <= 1);
	assert(length1 + length2 == length);
	assert(length1 + padding <= 8);
	assert(length2 < STLK_MAX_WRITE - 6);
	assert(length1 > 0);
	assert(length2 > 0);
	// Filling CBW
	memset(&cbw, 0, sizeof(scsi_usb_cbw));
	cbw.transfer_length = length2 + padding;
	cbw.flags = 0x00;
	cbw.cblength = 0x10;
	cbw.cb[0] = 0xf4;
	cbw.cb[1] = 0x0a;
	memcpy(cbw.cb+2, block_size2, 2);
	memcpy(cbw.cb+6, block_start2, 2);
	memcpy(cbw.cb+8+padding, buffer, length1);
	if(padding) cbw.cb[8] = '\0';
	assert( stlink_send_cbw(pgm->dev_handle, &cbw) == 0);
	usleep(3000);
	if(length2) {
		// Sending the rest
		unsigned char tail[STLK_MAX_WRITE-6];
		memcpy(tail, buffer + length1, length2);
		if(padding) tail[length2] = '\1';
		int actual;
		int r = libusb_bulk_transfer(pgm->dev_handle,
				(2 | LIBUSB_ENDPOINT_OUT),
				tail,
				length2 + padding,
				&actual,
				0);
		assert(actual == length2 + padding);
	}
	// Reading status
	stlink_read_csw(pgm->dev_handle, &csw);
	assert(csw.status == 0);
	memset(cbw.cb+8, 0, 8);
	cbw.cb[8] = 0x01;
	cbw.cb[9] = 0x02;
	int result = stlink_swim_wait(pgm);
	return(result);
}

int stlink_swim_write_range(programmer_t *pgm, const stm8_device_t *device, unsigned char *buffer, unsigned int start, unsigned int length, const memtype_t memtype) {
	int i;
	stlink_init_session(pgm);
	stlink_swim_write_byte(pgm, 0x00, device->regs.CLK_CKDIVR);
    if(memtype == FLASH || memtype == EEPROM || memtype == OPT) {
        stlink_swim_write_byte(pgm, 0x00, device->regs.FLASH_IAPSR);
    }
    if(memtype == FLASH) {
        stlink_swim_write_byte(pgm, 0x56, device->regs.FLASH_PUKR);
        stlink_swim_write_byte(pgm, 0xae, device->regs.FLASH_PUKR); 
    }
    if(memtype == EEPROM || memtype == OPT) {
        stlink_swim_write_byte(pgm, 0xae, device->regs.FLASH_DUKR);
        stlink_swim_write_byte(pgm, 0x56, device->regs.FLASH_DUKR);
    }
    if(memtype == FLASH || memtype == EEPROM || memtype == OPT) {
        stlink_swim_write_byte(pgm, 0x56, device->regs.FLASH_IAPSR);
    }
    int flash_block_size = device->flash_block_size;
	for(i = 0; i < length; i+=flash_block_size) {
		unsigned char block[128];
		memset(block, 0, sizeof(block));
		int block_size = length - i;
		if(block_size > flash_block_size)
			block_size = flash_block_size;
		DEBUG_PRINT("Writing block %04x with size %d\n", start+i, block_size);
		memcpy(block, buffer+i, block_size);
		if(block_size < flash_block_size) {
			DEBUG_PRINT("Padding block %04x with %d zeroes\n",
					start+i,
					flash_block_size - block_size);
			block_size = flash_block_size;
		}
        if(memtype == FLASH || memtype == EEPROM || memtype == OPT) {
            stlink_swim_write_byte(pgm, 0x01, device->regs.FLASH_CR2);
            if(device->regs.FLASH_NCR2 != 0) { // Device have FLASH_NCR2 register
                stlink_swim_write_byte(pgm, 0xFE, device->regs.FLASH_NCR2);
            }
        }
		int result = stlink_swim_write_block(pgm, block, start + i, block_size, 0);
		if(result & STLK_FLAG_ERR)
			fprintf(stderr, "Write error\n");
	}
    if(memtype == FLASH || memtype == EEPROM || memtype == OPT) {
        stlink_swim_write_byte(pgm, 0x56, device->regs.FLASH_IAPSR);
    }
	stlink_finish_session(pgm);
	return(length);
}
