#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "adapter.h"
#include "error.h"
#include "swim.h"

#define B3(x) ((((uint32_t)(x)) >> 24) & 0xff)
#define B2(x) ((((uint32_t)(x)) >> 16) & 0xff)
#define B1(x) ((((uint32_t)(x)) >> 8) & 0xff)
#define B0(x) ((((uint32_t)(x)) >> 0) & 0xff)

#define STLINK_SWIM_OK          0x00
#define STLINK_SWIM_BUSY        0x01
#define STLINK_SWIM_NO_RESPONSE 0x04 // Target did not respond. SWIM not active?
#define STLINK_SWIM_BAD_STATE   0x05	 // ??

#define STLINK_MODE_DFU        0x00
#define STLINK_MODE_MASS       0x01
#define STLINK_MODE_DEBUG      0x02
#define STLINK_MODE_SWIM       0x03
#define STLINK_MODE_BOOTL0ADER 0x04

#define STLINK_CMD_GET_VERSION          0xf1
#define STLINK_CMD_DEBUG                0xf2
#define STLINK_CMD_DFU                  0xf3
#define STLINK_SWIM                     0xf4
#define STLINK_CMD_GET_CURRENT_MODE     0xf5
#define STLINK_CMD_GET_VDD              0xf7

#define STLINK_DEBUG_EXIT               0x21

#define STLINK_DFU_EXIT                 0x07

#define STLINK_SWIM_ENTER               0x00
#define STLINK_SWIM_EXIT                0x01
#define STLINK_SWIM_READ_CAP            0x02
#define STLINK_SWIM_SPEED               0x03
#define STLINK_SWIM_ENTER_SEQ           0x04
#define STLINK_SWIM_GEN_RST             0x05
#define STLINK_SWIM_RESET               0x06
#define STLINK_SWIM_ASSERT_RESET        0x07
#define STLINK_SWIM_DEASSERT_RESET      0x08
#define STLINK_SWIM_READSTATUS          0x09
#define STLINK_SWIM_WRITEMEM            0x0a
#define STLINK_SWIM_READMEM             0x0b
#define STLINK_SWIM_READBUF             0x0c
#define STLINK_SWIM_READ_BUFFERSIZE     0x0d

struct _adapter {
	struct adapter a;
	enum { STLinkV1, STLinkV2, STLinkV21, STLinkV3} type;
};

static int32_t usb_transfer_out(struct adapter *adp, uint8_t *buf, uint32_t length) {
	int bytes_transferred = 0;
	const int type = ((struct _adapter *)adp)->type;
	const int ep = (type == STLinkV21 || type == STLinkV3) ? 1 : 2;
	const int err = libusb_bulk_transfer(adp->dev, ep | LIBUSB_ENDPOINT_OUT, buf, length, &bytes_transferred, 0);
	if (err != 0) {
		ERR("libusb_bulk_transfer_out ERROR %d %s", err, libusb_error_name(err));
		return -1;
	}
	if (bytes_transferred != length) {
		ERR("IO error: expected %d bytes but %d bytes transferred\n", length, bytes_transferred);
		return -1;
	}
	return bytes_transferred;
}

static int bulk_out(struct adapter *adp, unsigned char *buf, unsigned int buf_len) {
	// large transfers are split into two parts. The first is 16 bytes and the second is the rest.
	const int32_t count = buf_len >= 16 ? 16 : buf_len;
	if (usb_transfer_out(adp, buf, count) != count) {
		ERR("usb_transfer_out failed");
		return -1;
	}
	buf += count;
	buf_len -= count;
	if (buf_len) {
		if (usb_transfer_out(adp, buf, buf_len) != buf_len) {
			ERR("usb_transfer_out failed");
			return -1;
		}
	}
	return 0;
}

static int32_t usb_transfer_in(struct adapter *adp, uint8_t *buf, uint32_t length) {
	int bytes_transferred = 0;
	const int ep = 1;
	const int err = libusb_bulk_transfer(adp->dev, ep | LIBUSB_ENDPOINT_IN, buf, length, &bytes_transferred, 0);
	if (err != 0) {
		DBG("libusb_bulk_transfer ERROR %d %s", err, libusb_error_name(err));
		return -1;
	}

	return bytes_transferred;
}

static int32_t bulk_in(struct adapter *adp, uint8_t *buf, int32_t length) {
	// DBG("MAX IN %d bytes", length);
	while (length) {
		const int bytes_transferred = usb_transfer_in(adp, buf, length);
		if (bytes_transferred <= 0) {
			// some error
			ERR("bad transfer");
			return -1;
		}
		length -= bytes_transferred;
		buf += bytes_transferred;
		if (length) {
			usleep(10000);
		}
	}

	return 0;
}

static int get_status(struct adapter *adp, uint8_t *stat) {
	uint8_t buf[16] = {0};
	memset(buf, 0, sizeof(buf));
	buf[0] = STLINK_SWIM;
	buf[1] = STLINK_SWIM_READSTATUS;
	if (bulk_out(adp, buf, 2)) {
		return -1;
	}

	if (bulk_in(adp, buf, 4)) {
		return -1;
	}

	*stat = buf[0];
	return 0;
}

static int wait_for_status(struct adapter *adp) {
	while (1) {
		uint8_t stat;
		if (get_status(adp, &stat)) {
			return -1;
		}

		if (stat == STLINK_SWIM_OK) {
			break;
		}

		if (stat == STLINK_SWIM_BUSY) {
			usleep(1000);
			continue;
		}

		DBG("BAD STATUS %02X", stat);
		return -1;
	}
	return 0;
}

static int swim_read_byte(struct adapter *adp, uint32_t addr, uint8_t *byte) {
	assert(addr <= 0x00FFFFFF);
	uint8_t buf[16] = {0};
	buf[0] = STLINK_SWIM;
	buf[1] = STLINK_SWIM_READMEM;
	buf[2] = 0x00;
	buf[3] = 0x01;
	buf[4] = B3(addr);
	buf[5] = B2(addr);
	buf[6] = B1(addr);
	buf[7] = B0(addr);
	if (bulk_out(adp, buf, 8)) {
		return -1;
	}

	if (wait_for_status(adp)) {
		return -1;
	}

	buf[0] = STLINK_SWIM;
	buf[1] = STLINK_SWIM_READBUF;
	if (bulk_out(adp, buf, 2)) {
		return -1;
	}
	return bulk_in(adp, byte, 1);
}

static int swim_write_byte(struct adapter *adp, uint8_t byte, uint32_t addr) {
	assert(addr <= 0x00FFFFFF);
	unsigned char buf[16];
	buf[0] = STLINK_SWIM;
	buf[1] = STLINK_SWIM_WRITEMEM;
	buf[2] = 0x00;
	buf[3] = 0x01;
	buf[4] = B3(addr);
	buf[5] = B2(addr);
	buf[6] = B1(addr);
	buf[7] = B0(addr);
	buf[8] = byte;
	int err = bulk_out(adp, buf, 9);
	if (err) {
		return -1;
	}

	if (wait_for_status(adp)) {
		return -1;
	}
	return 0;
}

static int swim_gen_reset(struct adapter *adp) {
	unsigned char buf[8];
	buf[0] = STLINK_SWIM;
	buf[1] = STLINK_SWIM_GEN_RST;
	if (bulk_out(adp, buf, 2)) {
		return -1;
	}
	return 0;
}

void stlink2_close(struct adapter *adp) {
	unsigned char buf[8];
	buf[0] = STLINK_SWIM;
	buf[1] = STLINK_SWIM_EXIT;
	bulk_out(adp, buf, 2);
}

// Switch to high speed SWIM format (UM0470: 3.3)
static int stlink2_high_speed(struct adapter *adp) {
	unsigned char csr;

	// Wait for HSIT to be set in SWIM_CSR
	// FIXME: when does this happen? what triggers it?
	if (swim_read_byte(adp, SWIM_CSR, &csr)) {
		ERR("FAIL");
		return -1;
	}
	DBG("CSR: 0x%02X", csr);

	if (csr & SWIM_CSR_HSIT) {
		DBG("WRITE HS");
		// Set HS in SWIM_CSR
		if (swim_write_byte(adp, csr | SWIM_CSR_HS, SWIM_CSR)) {
			ERR("FAIL");
			return -1;
		}

		// Finally, tell the stlinkv2 to use high speed format.
		uint8_t buf[3] = {STLINK_SWIM, STLINK_SWIM_SPEED, 1};
		bulk_out(adp, buf, 3);
		DBG("continuing in high speed swim\n");
	} else {
		DBG("continuing in low speed swim\n");
	}
	
	return 0;
}

static int swim_enter_seq(struct adapter *adp) {
	unsigned char buf[2] = {0};
	buf[0] = STLINK_SWIM;
	buf[1] = STLINK_SWIM_ENTER_SEQ;
	if (bulk_out(adp, buf, 2)) {
		ERR("communication failed");
		return -1;
	}
	if (wait_for_status(adp)) {
		return -1;
	}
	return 0;
}

static int swim_assert_reset(struct adapter *adp) {
	unsigned char buf[2] = {0};
	buf[0] = STLINK_SWIM;
	buf[1] = STLINK_SWIM_ASSERT_RESET;
	if (bulk_out(adp, buf, 2)) {
		ERR("communication failed");
		return -1;
	}
	if (wait_for_status(adp)) {
		return -1;
	}
	return 0;
}

static int swim_deassert_reset(struct adapter *adp) {
	unsigned char buf[2] = {0};
	buf[0] = STLINK_SWIM;
	buf[1] = STLINK_SWIM_DEASSERT_RESET;
	if (bulk_out(adp, buf, 2)) {
		ERR("communication failed");
		return -1;
	}
	if (wait_for_status(adp)) {
		return -1;
	}
	return 0;
}

static int swim_connect(struct adapter *adp) {
	unsigned char buf[16] = {0};

	buf[0] = STLINK_CMD_GET_VERSION;
	if (bulk_out(adp, buf, 1)) {
		ERR("communication failed");
		return -1;
	}
	if (bulk_in(adp, buf, 6)) {
		ERR("communication failed");
		return -1;
	}
	unsigned int v = (buf[0] << 8) | buf[1];
	fprintf(stderr, "STLink: v%d, JTAG: v%d, SWIM: v%d, VID: %02x%02x, PID: %02x%02x\n", (v >> 12) & 0x3f, (v >> 6) & 0x3f, v & 0x3f, buf[2], buf[3], buf[4], buf[5]);

	// ask for current mode
	buf[0] = STLINK_CMD_GET_CURRENT_MODE;
	if (bulk_out(adp, buf, 1)) {
		ERR("communication failed");
		return -1;
	}
	if (bulk_in(adp, buf, 2)) {
		ERR("communication failed");
		return -1;
	}

	// if we are in one of the special modes... exit them
	switch (buf[0]) {
	case STLINK_MODE_DEBUG:
		buf[0] = STLINK_CMD_DEBUG;
		buf[1] = STLINK_DEBUG_EXIT;
		if (bulk_out(adp, buf, 2)) {
			ERR("communication failed");
			return -1;
		}
		break;

	case STLINK_MODE_BOOTL0ADER:
	case STLINK_MODE_DFU:
	case STLINK_MODE_MASS:
		buf[0] = STLINK_CMD_DFU;
		buf[1] = STLINK_DFU_EXIT;
		if (bulk_out(adp, buf, 2)) {
			ERR("communication failed");
			return -1;
		}
		break;

	default:
		break;
	}

	// not sure why this is needed
	buf[0] = STLINK_SWIM;
	buf[1] = STLINK_SWIM_ENTER;
	if (bulk_out(adp, buf, 2)) {
		ERR("communication failed");
		return -1;
	}

	// ask stlink how many bytes can be read/written in one go
	buf[0] = STLINK_SWIM;
	buf[1] = STLINK_SWIM_READ_BUFFERSIZE;
	if (bulk_out(adp, buf, 2)) {
		ERR("communication failed");
		return -1;
	}

	uint16_t bsize;
	if (bulk_in(adp, (uint8_t *)&bsize, 2)) {
		ERR("communication failed");
		return -1;
	}
	adp->read_buf_size = le16toh(bsize);

	DBG("adp->read_buf_size %d", adp->read_buf_size);

	// read capabilities... Is this usefull?
	buf[0] = STLINK_SWIM;
	buf[1] = STLINK_SWIM_READ_CAP;
	buf[2] = 0x01;
	if (bulk_out(adp, buf, 3)) {
		ERR("communication failed");
		return -1;
	}
	if (bulk_in(adp, buf, 8)) {
		ERR("communication failed");
		return -1;
	}
	DBG("CAP -> %02x %02x %02x %02x %02x %02x %02x %02x", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

	// FIXME:maybe get the target voltage??

	// pull reset line. Some parts don't have this but we do it anyway
	//FIXME: reset may remain asserted if any of the following commands fail
	if (swim_assert_reset(adp)) {
		ERR("communication failed");
		return -1;
	}

	// we use the enter sequence to get access to the SWIM module
	if (swim_enter_seq(adp)) {
		return -1;
	}

	// Mask internal interrupt sources, enable access to whole of memory,
	// prioritize SWIM and stall the CPU.
	// RST will be 0 so a SWIM SRST will not exit SWIM mode
	uint8_t csr = SWIM_CSR_SAFE_MASK|SWIM_CSR_SWIM_DM|SWIM_CSR_PRI;
	if (adp->swim_wotf(adp, &csr, 1, SWIM_CSR)) {
		ERR("Failed to set flags");
		return -1;
	}

	// HALT CPU via debug module
	csr = SWIM_DM_CSR2_STALL;
	if (adp->swim_wotf(adp, &csr, 1, SWIM_DM_CSR2)) {
		ERR("Failed to set flags");
		return -1;
	}
	
	// generate a reset to cause option bytes to load
	// this will also start the internal clock and HSIT will be set allowing us to enter high speed mode
	if (swim_gen_reset(adp)) {
		ERR("UNABLE TO GENERATE RESET");
		return -1;
	}
	
	// release reset
	if (swim_deassert_reset(adp)) {
		ERR("communication failed");
		return -1;
	}

	usleep(1000);

	DBG("ATEMPT ENTER BIGH SPEED");
	return stlink2_high_speed(adp);
}

static int swim_wotf(struct adapter *adp, const uint8_t *buf, uint32_t length, uint32_t addr) {
	unsigned int remaining = length;

	while (remaining > 0) {
		unsigned int size = (remaining > adp->read_buf_size ? adp->read_buf_size : remaining);

		DBG("write [0x%04x to 0x%04x] %d", addr, addr + size, size);

		const int psize = 8 + size;
		uint8_t *tmp = malloc(psize + 4);
		assert(tmp);
		tmp[0] = STLINK_SWIM;
		tmp[1] = STLINK_SWIM_WRITEMEM;
		tmp[2] = B1(length);
		tmp[3] = B0(length);
		tmp[4] = B3(addr);
		tmp[5] = B2(addr);
		tmp[6] = B1(addr);
		tmp[7] = B0(addr);
		memcpy(&tmp[8], buf, size);

		if (bulk_out(adp, tmp, psize)) {
			ERR("FAIL");
			free(tmp);
			return -1;
		}

		free(tmp);

		if (wait_for_status(adp)) {
			ERR("FAILED TO WAIT FOR STATUS");
			return -1;
		}

		buf += size;
		addr += size;
		remaining -= size;
	}
	return 0;
}

static int swim_rotf(struct adapter *adp, uint32_t addr, uint8_t *buf, uint32_t length) {
	uint8_t buff[16];
	unsigned int remaining = length;

	while (remaining > 0) {
		unsigned int size = (remaining > adp->read_buf_size ? adp->read_buf_size : remaining);

		DBG("read [0x%04x to 0x%04x] %d", addr, addr + size, size);

		buff[0] = STLINK_SWIM;
		buff[1] = STLINK_SWIM_READMEM;
		buff[2] = B1(size);
		buff[3] = B0(size);
		buff[4] = B3(addr);
		buff[5] = B2(addr);
		buff[6] = B1(addr);
		buff[7] = B0(addr);
		bulk_out(adp, buff, 8);

		if (wait_for_status(adp)) {
			return -1;
		}

		buff[0] = STLINK_SWIM;
		buff[1] = STLINK_SWIM_READBUF;
		bulk_out(adp, buff, 2);

		if (bulk_in(adp, buf, size)) {
			ERR("communication failed");
			return -1;
		}

		buf += size;
		addr += size;
		remaining -= size;
	}
	return 0;
}

static int swim_srst(struct adapter *adp) {
	// SWIM srst only works when the device is in debug mode
	uint8_t csr;
	if (swim_read_byte(adp, SWIM_CSR, &csr)) {
		ERR("failed to get SIM_CSR");
		return -1;
	}
	
	DBG("SWIM_CSR: 0x%02X");
	if ((csr & SWIM_CSR_SWIM_DM) == 0) {
		ERR("SWIM NOT IN DEBUG MODE");
		return -1;
	}

	unsigned char buf[8];
	buf[0] = STLINK_SWIM_GEN_RST;
	return bulk_out(adp, buf, 1);
}

const static struct adapter adapter = {
		.swim_connect = swim_connect,
		.swim_rotf = swim_rotf,
		.swim_wotf = swim_wotf,
		.swim_srst = swim_srst,
		.swim_assert_reset = swim_assert_reset,
		.swim_deassert_reset = swim_deassert_reset,
		// FIXME CLOSE ?
};

static struct _adapter *_adapter_base(libusb_context *ctx, libusb_device_handle *dev) {
	struct _adapter *_adp = (struct _adapter *)malloc(sizeof(*_adp));
	if (_adp) {
		struct adapter *adp = &_adp->a;
		*adp = adapter;
		adp->ctx = ctx;
		adp->dev = dev;

		if (libusb_kernel_driver_active(adp->dev, 0) == 1) { // find out if kernel driver is attached
			if (libusb_detach_kernel_driver(adp->dev, 0)) {
				ERR("unable to detach kernel driver.");
				free(adp);
				return NULL;
			}
		}

		if (libusb_claim_interface(adp->dev, 0)) {
			ERR("unable to claim interface.");
			free(adp);
			return NULL;
		}
	}
	return _adp;
}

struct adapter *stlinkv2_adapter_open(libusb_context *ctx, libusb_device_handle *dev) {
	struct _adapter *_adp = _adapter_base(ctx, dev);
	if (_adp) {
		_adp->type = STLinkV2;
	}
	return (struct adapter *)_adp;
}

struct adapter *stlinkv21_adapter_open(libusb_context *ctx, libusb_device_handle *dev) {
	struct _adapter *_adp = _adapter_base(ctx, dev);
	if (_adp) {
		_adp->type = STLinkV21;
	}
	return (struct adapter *)_adp;
}

