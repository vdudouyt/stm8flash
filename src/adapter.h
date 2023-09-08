#ifndef __ADAPTER_H
#define __ADAPTER_H

#include <stdint.h>
#if defined(WIN32) || defined(__CYGWIN__)
#include <libusb-1.0/libusb.h>
#else
#include <libusb.h>
#endif

struct adapter {
	// connect to MCU
	int (*swim_connect)(struct adapter *adp);
	int (*swim_rotf)(struct adapter *adp, uint32_t addr, uint8_t *buf, uint32_t length);
	int (*swim_wotf)(struct adapter *adp, const uint8_t *buf, uint32_t length, uint32_t addr);
	int (*swim_srst)(struct adapter *adp);
	int (*swim_assert_reset)(struct adapter *adp);
	int (*swim_deassert_reset)(struct adapter *adp);

	libusb_device_handle *dev;
	libusb_context *ctx;
	uint16_t read_buf_size;
};

struct adapter *stlinkv2_adapter_open(libusb_context *ctx, libusb_device_handle *dev_handle);
struct adapter *stlinkv21_adapter_open(libusb_context *ctx, libusb_device_handle *dev_handle);

static const struct adapter_info {
	const char *name;
	uint16_t usb_vid;
	uint16_t usb_pid;
	struct adapter *(*open)(libusb_context *ctx, libusb_device_handle *dev_handle);
} adapter_info[3] = {
	{"stlinkv2", 0x0483, 0x3748, stlinkv2_adapter_open},
	{"stlinkv21", 0x0483, 0x374b, stlinkv21_adapter_open},
	{NULL}
};

#endif
