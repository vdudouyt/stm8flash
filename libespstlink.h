/**
 * libespstlink provides low level access to the STM8 SWIM protocol using
 * the espstlink hardware (https://github.com/rumpeltux/esp-stlink)
 */
#ifndef __LIBESPSTLINKV_H
#define __LIBESPSTLINKV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct _espstlink_t {
  int fd;
  int version;
} espstlink_t;

typedef struct _esplink_error_t {
  int code;
  char *message;
  char data[256];
  size_t data_len;
  int device_code;
} espstlink_error_t;

#define ESPSTLINK_ERROR_READ 1
#define ESPSTLINK_ERROR_DATA 2
#define ESPSTLINK_ERROR_COMM 3
#define ESPSTLINK_ERROR_VERSION 4

#define ESPSTLINK_SWIM_ERROR_READ_BIT_TIMEOUT -1
#define ESPSTLINK_SWIM_ERROR_INVALID_TARGET_ID -2
#define ESPSTLINK_SWIM_ERROR_PARITY -3
#define ESPSTLINK_SWIM_ERROR_NACK -4
#define ESPSTLINK_SWIM_ERROR_SYNC_TIMEOUT_1 -5
#define ESPSTLINK_SWIM_ERROR_SYNC_TIMEOUT_2 -6

espstlink_error_t *espstlink_get_last_error();

espstlink_t *espstlink_open(const char *device);
void espstlink_close(espstlink_t *pgm);
bool espstlink_fetch_version(espstlink_t *pgm);

bool espstlink_swim_entry(const espstlink_t *pgm);
bool espstlink_swim_srst(const espstlink_t *pgm);
bool espstlink_swim_read(const espstlink_t *pgm, uint8_t *buffer,
                         unsigned int addr, size_t size);
bool espstlink_swim_write(const espstlink_t *pgm, const uint8_t *buffer,
                          unsigned int addr, size_t size);

/**
 * Switch the reset pin.
 * If `input`, the pin is used as an input pin with a pull-up resistor.
 * Otherwise, the pin is used as an output pin as follows:
 * value == 0: DEFAULT, sets the pin HIGH
 * value == 1: RESET, sets the pin LOW
 */
bool espstlink_reset(const espstlink_t *pgm, bool input, bool enable_reset);
#endif
