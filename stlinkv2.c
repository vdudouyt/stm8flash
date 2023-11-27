/* stlink-v2 specific functions
   (c) Valentin Dudouyt, 2012-2013 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "stlink.h"
#include "error.h"
#include "try.h"
#include "byte_utils.h"
#include "stlinkv2.h"
#include "utils.h"


/* Use high speed SWIM mode.
 * This changes the ratio used in bit signalling from 2:20 to 2:8. Since it neither
 * changes the clock rate nor changes the smallest interval between edges there
 * seems little reason not to always use it.
 */
#define USE_HIGH_SPEED          1

/* Only write differences to the target.
 * If set to 1 then a write operation first reads the relevant memory from the
 * target then only writes back those blocks that contain actual changes thus
 * sparing flash from unnecessary erase-and-rewrite cycles.
 */
#define ONLY_WRITE_DIFFS        1


#define MAX_SWIM_ERRORS         8

#define STLINK_SWIM_OK          0x00
#define STLINK_SWIM_BUSY        0x01
#define STLINK_SWIM_NO_RESPONSE 0x04 // Target did not respond. SWIM not active?
#define STLINK_SWIM_BAD_STATE   0x05 // ??

#define STLINK_MODE_DFU	        0x00
#define STLINK_MODE_MASS        0x01
#define STLINK_MODE_DEBUG       0x02
#define STLINK_MODE_SWIM        0x03
#define STLINK_MODE_BOOTLOADER  0x04

#define STLINK_GET_VERSION      0xf1
#define STLINK_DEBUG            0xf2
#define STLINK_DFU              0xf3
#define STLINK_SWIM             0xf4
#define STLINK_GET_CURRENT_MODE 0xf5
#define STLINK_GET_VDD          0xf7

#define DEBUG_EXIT              0x21
#define DFU_EXIT                0x07

#define SWIM_ENTER              0x00
#define SWIM_EXIT               0x01
#define SWIM_READ_CAP           0x02
#define SWIM_SPEED              0x03
#define SWIM_ENTER_SEQ          0x04
#define SWIM_GEN_RST            0x05
#define SWIM_RESET              0x06
#define SWIM_ASSERT_RESET       0x07
#define SWIM_DEASSERT_RESET     0x08
#define SWIM_READSTATUS         0x09
#define SWIM_WRITEMEM           0x0a
#define SWIM_READMEM            0x0b
#define SWIM_READBUF            0x0c
#define SWIM_READBUFSIZE        0x0d

#if DEBUG
#undef nSTR
#define nSTR(name)	[name] = #name + 6
static const char * const debug_cmd_map[] = {
	nSTR(DEBUG_EXIT),
};

#undef nSTR
#define nSTR(name)	[name] = #name + 4
static const char * const dfu_cmd_map[] = {
	nSTR(DFU_EXIT),
};

#undef nSTR
#define nSTR(name)	[name] = #name + 5
static const char * const swim_cmd_map[] = {
	nSTR(SWIM_ENTER),
	nSTR(SWIM_EXIT),
	nSTR(SWIM_READ_CAP),
	nSTR(SWIM_SPEED),
	nSTR(SWIM_ENTER_SEQ),
	nSTR(SWIM_GEN_RST),
	nSTR(SWIM_RESET),
	nSTR(SWIM_ASSERT_RESET),
	nSTR(SWIM_DEASSERT_RESET),
	nSTR(SWIM_READSTATUS),
	nSTR(SWIM_WRITEMEM),
	nSTR(SWIM_READMEM),
	nSTR(SWIM_READBUF),
	nSTR(SWIM_READBUFSIZE),
};

#undef nSTR
#define nSTR(name)	[name - STLINK_GET_VERSION] = #name + 7
static const char * const stlink_cmd_map[] = {
	nSTR(STLINK_GET_VERSION),
	nSTR(STLINK_DEBUG),
	nSTR(STLINK_DFU),
	nSTR(STLINK_SWIM),
	nSTR(STLINK_GET_CURRENT_MODE),
	nSTR(STLINK_GET_VDD),
};

static const char * const cmd_to_str(unsigned int cmd) {
	static char buf[3];

	cmd -= STLINK_GET_VERSION;
	if (cmd < sizeof(stlink_cmd_map)/sizeof(stlink_cmd_map[0]) && stlink_cmd_map[cmd])
		return stlink_cmd_map[cmd];

	sprintf(buf, "%02x", cmd + STLINK_GET_VERSION);
	return buf;
}


static const struct {
	const char * const * cmd_to_str;
	size_t size;
} stlink_subcmd_map[] = {
	[STLINK_GET_VERSION - STLINK_GET_VERSION] = { NULL, 0 },
	[STLINK_DEBUG - STLINK_GET_VERSION] = { debug_cmd_map, sizeof(debug_cmd_map) / sizeof(debug_cmd_map[0]) },
	[STLINK_DFU - STLINK_GET_VERSION] = { dfu_cmd_map, sizeof(dfu_cmd_map) / sizeof(dfu_cmd_map[0]) },
	[STLINK_SWIM - STLINK_GET_VERSION] = { swim_cmd_map, sizeof(swim_cmd_map) / sizeof(swim_cmd_map[0]) },
	[STLINK_GET_CURRENT_MODE - STLINK_GET_VERSION] = { NULL, 0 },
	[STLINK_GET_VDD - STLINK_GET_VERSION] = { NULL, 0 },
};

static const char * const subcmd_to_str(unsigned int cmd, unsigned int subcmd) {
	static char buf[3];

	cmd -= STLINK_GET_VERSION;
	if (cmd < sizeof(stlink_subcmd_map)/sizeof(stlink_subcmd_map[0]) && subcmd < stlink_subcmd_map[cmd].size)
		return stlink_subcmd_map[cmd].cmd_to_str[subcmd];

	sprintf(buf, "%02x", subcmd);
	return buf;
}
#endif

unsigned char *pack_int16(uint16_t word, unsigned char *out);

unsigned int read_buf_size = 6144;

static void swim_write_byte(programmer_t *pgm, unsigned char byte, unsigned int start);
static int swim_read_byte(programmer_t *pgm, unsigned int addr);

static unsigned int msg_transfer(programmer_t *pgm, unsigned char *buf, unsigned int length, int direction) {
	int bytes_transferred = 0;
	int ep = (direction == LIBUSB_ENDPOINT_OUT) ? 2 : 1;
	if (pgm->type == STLinkV21 || pgm->type == STLinkV3)
		ep = 1;
	libusb_bulk_transfer(pgm->dev_handle, ep | direction, buf, length, &bytes_transferred, 0);
	if(bytes_transferred != length) ERROR2("IO error: expected %d bytes but %d bytes transferred\n", length, bytes_transferred);
	return bytes_transferred;
}

static void msg_send(programmer_t *pgm, unsigned char *buf, unsigned int length) {
	while (length > 0) {
		int n = msg_transfer(pgm, buf, length, LIBUSB_ENDPOINT_OUT);

		length -= n;
		buf += n;

		if (length > 0) {
			DEBUG_PRINT("    short write - %d bytes still to go\n", length);
			usleep(1000000);
		}
	}
}

static void msg_recv(programmer_t *pgm, unsigned char *buf, unsigned int length) {
	int n;

	while (length > 0) {
		n = msg_transfer(pgm, buf, length, LIBUSB_ENDPOINT_IN);
		length -= n;
		buf += n;

		if (length > 0) {
			DEBUG_PRINT("    short read - %d bytes more needed\n", length);
			usleep(1000000);
		}
	}
}

static unsigned int msg_recv_int(programmer_t *pgm, unsigned int length) {
	unsigned char buf[4] = { 0x00, 0x01, 0x02, 0x03 };
	msg_recv(pgm, buf, length);
	unsigned int ret = load_int(buf, length, MP_LITTLE_ENDIAN);
	DEBUG_PRINT("        -> 0x%x\n", ret);
	return ret;
}

static unsigned int msg_recv_int8(programmer_t *pgm) {	return msg_recv_int(pgm, 1); }
static unsigned int msg_recv_int16(programmer_t *pgm) {	return msg_recv_int(pgm, 2); }

static void stlink2_cmd_internal(programmer_t *pgm, unsigned char *buf, unsigned int buf_len, unsigned int length, va_list ap) {
	unsigned char cmd_buf[16];
	int i, j;

	// Preparing
	memset(cmd_buf, 0, sizeof(cmd_buf));
	for(i = 0; i < length; i++) {
		int arg = va_arg(ap, int);
		cmd_buf[i] = arg;
	}

	while (buf_len > 0 && i < sizeof(cmd_buf)) {
		cmd_buf[i++] = *(buf++);
		buf_len--;
	}

	DEBUG_PRINT("     %s", cmd_to_str(cmd_buf[0]));
	if (i > 1)
		DEBUG_PRINT(" %s", subcmd_to_str(cmd_buf[0], cmd_buf[1]));
	for (j = 2; j < i; j++)
		DEBUG_PRINT(" %02x", cmd_buf[j]);
	if (buf_len > 0)
		DEBUG_PRINT(" + %d more bytes", buf_len);
	DEBUG_PRINT("\n");

	// Triggering USB transfer
	msg_send(pgm, cmd_buf, sizeof(cmd_buf));
	if (buf_len)
		msg_send(pgm, buf, buf_len);
}

static void stlink2_cmd(programmer_t *pgm, unsigned int length, ...) {
	va_list ap;

	va_start(ap, length);
	stlink2_cmd_internal(pgm, NULL, 0, length, ap);
	va_end(ap);
}

static void swim_cmd_internal(programmer_t *pgm, unsigned char *buf, unsigned int buf_len, unsigned int length, va_list ap) {
	int stalls = 0;
	unsigned char status[2][4];
	int set = 0;

	stlink2_cmd_internal(pgm, buf, buf_len, length, ap);

	while (stalls < 4) {

		stlink2_cmd(pgm,2,STLINK_SWIM,SWIM_READSTATUS);
		msg_recv(pgm, status[set], 4);
		DEBUG_PRINT("        status %02x %02x %02x %02x\n", status[set][0], status[set][1], status[set][2], status[set][3]);

		if (status[set][0] == STLINK_SWIM_OK) {
			// We're done!
			return;
		}

		// Still waiting...
		if (memcmp(status[0], status[1], 4))
			stalls = 0;
		else
			stalls++;

		set ^= 1;
		usleep(10000);
		//usleep(100);
	}
	ERROR2("SWIM error 0x%02d\n", status[set][0]);
}

static void swim_cmd(programmer_t *pgm, unsigned int length, ...) {
	va_list ap;

	va_start(ap, length);
	swim_cmd_internal(pgm, NULL, 0, length, ap);
	va_end(ap);
}

static void swim_cmd_with_data(programmer_t *pgm, unsigned char *buf, unsigned int buf_len, unsigned int length, ...) {
	va_list ap;

	va_start(ap, length);
	swim_cmd_internal(pgm, buf, buf_len, length, ap);
	va_end(ap);
}

#if USE_HIGH_SPEED
// Switch to high speed SWIM format (UM0470: 3.3)
static void stlink2_high_speed(programmer_t *pgm) {
	unsigned char csr;

	// Wait for HSIT to be set in SWIM_CSR
	// avoid hanging when HSIT doesn't become 1
	unsigned char retries = 10;
	while (!((csr = swim_read_byte(pgm, 0x7f80)) & 0x02) && (retries-- != 0))
		usleep(500);

	// Do a SWIM_RESET to resync clocking
	swim_cmd(pgm, 2, STLINK_SWIM, SWIM_RESET);

	if (csr & 0x02) {
		// Set HS in SWIM_CSR
		swim_write_byte(pgm, csr | 0x10, 0x7f80);

		// Finally, tell the stlinkv2 to use high speed format.
		swim_cmd(pgm, 3, STLINK_SWIM, SWIM_SPEED, 1);
		DEBUG_PRINT("continuing in high speed swim\n");
	}
	else {
		DEBUG_PRINT("continuing in low speed swim\n");
	}
}
#endif

bool stlink2_open(programmer_t *pgm) {
	unsigned char buf[8];
	unsigned int v;

	stlink2_cmd(pgm, 1, STLINK_GET_VERSION);
	msg_recv(pgm, buf, 6);
	v = (buf[0] << 8) | buf[1];
	fprintf(stderr, "STLink: v%d, JTAG: v%d, SWIM: v%d, VID: %02x%02x, PID: %02x%02x\n",
		(v >> 12) & 0x3f, (v >> 6) & 0x3f, v & 0x3f, buf[2], buf[3], buf[4], buf[5]);

#if 0
	// This does not appear to work on all ST-Link V2 clones even if the JTAG
	// version is high enough?
	if (((v >> 6) & 0x3f) >= 13) {
		stlink2_cmd(pgm, 1, STLINK_GET_VDD);
		msg_recv(pgm, buf, 8);
		if ((v = load_int(buf, 4, MP_LITTLE_ENDIAN)))
			fprintf(stderr, "Target voltage: %.2fV\n", 2.0 * (load_int(buf+4, 4, MP_LITTLE_ENDIAN) * (1.2 / v)));
	}
#endif

	stlink2_cmd(pgm, 1, STLINK_GET_CURRENT_MODE);
	msg_recv(pgm, buf, 2);
	DEBUG_PRINT("        -> %02x %02x\n", buf[0], buf[1]);

	switch (buf[0]) {
		case STLINK_MODE_DEBUG:
			stlink2_cmd(pgm, 2, STLINK_DEBUG, DEBUG_EXIT);
			break;

		case STLINK_MODE_BOOTLOADER:
		case STLINK_MODE_DFU:
		case STLINK_MODE_MASS:
			stlink2_cmd(pgm, 2, STLINK_DFU, DFU_EXIT);
			break;

		default:
			break;
	}

	if (buf[0] != STLINK_MODE_SWIM)
		stlink2_cmd(pgm, 2, STLINK_SWIM, SWIM_ENTER);

	stlink2_cmd(pgm, 2, STLINK_SWIM, SWIM_READBUFSIZE);
	read_buf_size = msg_recv_int16(pgm);

	stlink2_cmd(pgm, 3, STLINK_SWIM, SWIM_READ_CAP, 0x01);
	msg_recv(pgm, buf, 8);
	DEBUG_PRINT("        -> %02x %02x %02x %02x %02x %02x %02x %02x\n",
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

	swim_cmd(pgm, 2, STLINK_SWIM, SWIM_ASSERT_RESET);

	swim_cmd(pgm, 2, STLINK_SWIM, SWIM_ENTER_SEQ);

	// Mask internal interrupt sources, enable access to whole of memory,
	// prioritize SWIM and stall the CPU.
	swim_write_byte(pgm, 0xa1, 0x7f80);

	swim_cmd(pgm, 2, STLINK_SWIM, SWIM_DEASSERT_RESET);
	usleep(1000);

#if USE_HIGH_SPEED
	stlink2_high_speed(pgm);
#endif

	return(true);
}

void stlink2_srst(programmer_t *pgm) {
	swim_write_byte(pgm, swim_read_byte(pgm, 0x7f80) | 0x4, 0x7f80); // set SWIM_CSR.RST
	// alt : remove stall bit after reset (like libespstlink)
	swim_cmd(pgm, 2, STLINK_SWIM, SWIM_GEN_RST);
	usleep(1000);
}

void stlink2_close(programmer_t *pgm) {
	stlink2_cmd(pgm, 2, STLINK_SWIM, SWIM_EXIT);
}

static void swim_write_byte(programmer_t *pgm, unsigned char byte, unsigned int start) {
	swim_cmd(pgm, 9, STLINK_SWIM, SWIM_WRITEMEM,
			0x00, 0x01,
			0x00, EX(start),
			HI(start), LO(start),
			byte);
}

#if 0
static void stlink2_write_word(programmer_t *pgm, unsigned int word, unsigned int start) {
	swim_cmd(pgm, 10, STLINK_SWIM, SWIM_WRITEMEM,
			0x00, 0x02,
			0x00, EX(start),
			HI(start), LO(start),
			HI(word), LO(word));
}
#endif

static int swim_read_byte(programmer_t *pgm, unsigned int addr) {
	swim_cmd(pgm, 8, STLINK_SWIM, SWIM_READMEM,
			0x00, 0x01,
			0x00, EX(addr),
			HI(addr), LO(addr));

	stlink2_cmd(pgm, 2, STLINK_SWIM, SWIM_READBUF);
	return(msg_recv_int8(pgm));
}

int stlink2_swim_read_range(programmer_t *pgm, const stm8_device_t *device, unsigned char *buffer, unsigned int start, unsigned int length) {
	DEBUG_PRINT("read range\n");

	unsigned int remaining = length;

	while (remaining > 0) {
		unsigned int size = (remaining > read_buf_size ? read_buf_size : remaining);

		DEBUG_PRINT("read 0x%04x to 0x%04x\n", start, start + size);

		swim_cmd(pgm, 8, STLINK_SWIM, SWIM_READMEM,
				HI(size), LO(size),
				0x00, EX(start), HI(start), LO(start));

		stlink2_cmd(pgm, 2, STLINK_SWIM, SWIM_READBUF);
		msg_recv(pgm, buffer, size);

		buffer += size;
		start += size;
		remaining -= size;
	}

	return length;
}

int stlink2_swim_write_range(programmer_t *pgm, const stm8_device_t *device, unsigned char *buffer, unsigned int start, unsigned int length, const memtype_t memtype) {
	unsigned int iapsr;

	DEBUG_PRINT("write range: setup\n");

	swim_write_byte(pgm, 0x00, device->regs.CLK_CKDIVR);

	// Unlock MASS
	if (memtype == FLASH) {
		DEBUG_PRINT("write range: unlock FLASH\n");
		swim_write_byte(pgm, 0x56, device->regs.FLASH_PUKR);
		swim_write_byte(pgm, 0xae, device->regs.FLASH_PUKR); 
	} else if (memtype == EEPROM || memtype == OPT) {
		DEBUG_PRINT("write range: unlock EEPROM\n");
		swim_write_byte(pgm, 0xae, device->regs.FLASH_DUKR);
		swim_write_byte(pgm, 0x56, device->regs.FLASH_DUKR);
	}

	if (memtype == OPT) {
		// Option programming mode
		swim_write_byte(pgm, 0x80, device->regs.FLASH_CR2);
		if (device->regs.FLASH_NCR2 != 0) {
			swim_write_byte(pgm, 0x7F, device->regs.FLASH_NCR2);
		}

		for (unsigned int i = 0; i < length; i++) {
			swim_write_byte(pgm, *(buffer++), start++);
			// Wait for EOP to be set in FLASH_IAPSR
			usleep(6000); // t_prog per the datasheets is 6ms typ, 6.6ms max
			TRY(5,swim_read_byte(pgm, device->regs.FLASH_IAPSR) & 0x04);
		}

	} else {
		// NOTE : RAM is also written in flash_block_size chunks here; just for convenience
		unsigned int rounded_size = ((length - 1) / device->flash_block_size + 1) * device->flash_block_size;
		unsigned char *current = alloca(rounded_size);
		int i;

#if ONLY_WRITE_DIFFS
		stlink2_swim_read_range(pgm, device, current, start, rounded_size);
		memcpy(buffer + length, current + length, rounded_size - length);
#endif

		DEBUG_PRINT("write range: block program with block size = %d\n", device->flash_block_size);

		for (i = 0; i < length; i += device->flash_block_size) {
				// BUG HERE : can read beyond buffer[] array boundary, because buffer is not always a multiple of flash_block_size
			if (ONLY_WRITE_DIFFS && !memcmp(current + i, buffer + i, device->flash_block_size)) {
				DEBUG_PRINT("no change 0x%04x to 0x%04x\n", start + i, start + i + device->flash_block_size);
			} else {
				int prgmode;
				/*
				 * Use fast block programming (prgmode = 0x10) only if we have
				 * read the flash block and verified that it is empty (all its
				 * bytes are 0x00).
				 */
#if ONLY_WRITE_DIFFS
				prgmode = 0x10;
				for (int j = 0; j < device->flash_block_size; j++) {
					if (current[i + j]) {
						prgmode = 0x01;
						break;
					}
				}
#else
				prgmode = 0x01;
#endif

				DEBUG_PRINT("%swrite 0x%04x to 0x%04x\n", (prgmode == 0x10 ? "fast " : ""), start + i, start + i + device->flash_block_size);

				if (memtype == FLASH || memtype == EEPROM) {
					// Stall the CPU before entering block programming mode
          // If the CPU keeps running and executes a software reset
          // (e.g. due to an invalid instruction) programming fails.
					int csr = swim_read_byte(pgm, device->regs.FLASH_DM_CSR2);
					swim_write_byte(pgm, csr | 8, device->regs.FLASH_DM_CSR2);

					// Block programming mode
					swim_write_byte(pgm, prgmode, device->regs.FLASH_CR2);
					if(device->regs.FLASH_NCR2 != 0) {
						swim_write_byte(pgm, ~prgmode, device->regs.FLASH_NCR2);
					}
				}

				// Page-based writing
				// The first 8 packet bytes are transmitted in the same USB bulk transfer
				// as the command itself with the rest following.
				// BUG HERE : only works correctly if start is on flash block boundary
				swim_cmd_with_data(pgm, buffer + i, device->flash_block_size, 8, STLINK_SWIM, SWIM_WRITEMEM,
					HI(device->flash_block_size), LO(device->flash_block_size),
					EH(start + i), EX(start + i), HI(start + i), LO(start + i));

				if (memtype == FLASH || memtype == EEPROM) {
					// Wait for EOP to be set in FLASH_IAPSR
					// t_prog per the datasheets is 6ms typ, 6.6ms max, fast mode is twice as fast
					usleep(prgmode == 0x10 ? 3000 : 6000);
					//TRY(5,swim_read_byte(pgm, device->regs.FLASH_IAPSR) & 0x04);
					// provide a better error message than 'tries exceeded'
					do {
						int retries = 5;
						int iapsr;
						while (retries > 0) {
							iapsr = swim_read_byte(pgm, device->regs.FLASH_IAPSR);
							if (iapsr & 0x04) break;
							if (iapsr & 0x01) {
								ERROR("target page is write protected (UBC) or read-out protection is enabled");
							}
							retries--;
							usleep(10000);
						}
					} while (0);
				}
			}
		}
	}

	if (memtype == FLASH || memtype == EEPROM || memtype == OPT) {
		// Reset DUL and PUL in IAPSR to disable flash and data writes.
		iapsr = swim_read_byte(pgm, device->regs.FLASH_IAPSR);
		swim_write_byte(pgm, iapsr & (~0x0a), device->regs.FLASH_IAPSR);
	}

	return(length);
}
