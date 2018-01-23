/**
 * Copyright (C) 2017 Hagen Fritsch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libespstlink.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

static espstlink_error_t error = {0, NULL};

espstlink_error_t *get_last_error() { return &error; }

/** Set the error message based on a format string. */
static void set_error(int code, char *format, ...) {
  // Free an old message.
  if (error.message != NULL) free(error.message);

  memset(&error, 0, sizeof(error));

  // Calculate required memory size
  va_list arglist;
  va_start(arglist, format);
  size_t size = vsnprintf(NULL, 0, format, arglist);
  va_end(arglist);

  // Store actual message.
  error.message = malloc(size);
  va_start(arglist, format);
  vsnprintf(error.message, size, format, arglist);
  va_end(arglist);
}

espstlink_t *espstlink_open(const char *device) {
  struct termios tty;
  memset(&tty, 0, sizeof tty);

  int fd = open(device == NULL ? "/dev/ttyUSB0" : device, O_RDWR | O_NOCTTY);
  if (fd < 0) {
    perror("Couldn't open tty");
    return NULL;
  }

  /* Error Handling */
  if (tcgetattr(fd, &tty) != 0) {
    perror("Couldn't open tty");
    return NULL;
  }

  /* Set Baud Rate */
  cfsetospeed(&tty, (speed_t)B115200);
  cfsetispeed(&tty, (speed_t)B115200);

  /* Setting other Port Stuff */
  tty.c_cc[VMIN] = 0;             // read does block
  tty.c_cc[VTIME] = 1;            // 0.1 seconds read timeout
  tty.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines

  /* Make raw */
  cfmakeraw(&tty);

  /* Flush Port, then applies attributes */
  tcflush(fd, TCIFLUSH);
  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    perror("Setting tty attributes failed");
    return NULL;
  }
  espstlink_t *pgm = malloc(sizeof(espstlink_t));
  pgm->fd = fd;
  return pgm;
}

static bool error_check(int fd, uint8_t command, uint8_t *resp_buf,
                        size_t size) {
  uint8_t buf[4];
  int len = read(fd, buf, 1);
  if (len < 1) {
    set_error(ESPSTLINK_ERROR_READ,
              "Didn't get a response from the device: %s\n", strerror(errno));
    return 0;
  }
  if (buf[0] != command) {
    set_error(ESPSTLINK_ERROR_DATA, "Unexpected data: %02x\n", buf[0]);
    error.data[0] = buf[0];
    error.data_len = 1 + read(fd, &error.data[1], sizeof(error.data) - 1);
    return 0;
  }
  len = read(fd, buf + 1, 1);
  if (len < 1) {
    set_error(ESPSTLINK_ERROR_DATA, "Device didn't finish command 0x%02x: %s\n",
              buf[0], strerror(errno));
    return 0;
  }
  if (buf[1] == 0) {
    if (resp_buf && size) {
      size_t total = 0;
      while (total < size) {
        len = read(fd, resp_buf + total, size);
        if (len < 1) {
          set_error(
              ESPSTLINK_ERROR_DATA,
              "Incomplete response for command 0x%02x: expected %d bytes, "
              "but got %d bytes (%s)\n",
              buf[0], size, len, strerror(errno));
          return 0;
        }
        total += len;
      }
    }
    return 1;
  }
  if (buf[1] == 0xFF) {
    len = read(fd, buf + 2, 2);
    if (len < 2) {
      set_error(
          ESPSTLINK_ERROR_DATA,
          "Device didn't finish sending error code for command 0x%02x: %s\n",
          buf[0], strerror(errno));
      return 0;
    }
    int code = buf[2] << 8 | buf[3];
    set_error(ESPSTLINK_ERROR_COMM, "Command 0x%02x failed with code: 0x%02x\n",
              buf[0], code);
    error.device_code = code;
  } else {
    set_error(ESPSTLINK_ERROR_DATA,
              "Unexpected error code for command 0x%02x: 0x%02x\n", buf[0],
              buf[1]);
    error.data[0] = buf[0];
    error.data[1] = buf[1];
    error.data_len = 2 + read(fd, &error.data[2], sizeof(error.data) - 2);
  }
  return 0;
}

bool espstlink_check_version(const espstlink_t *pgm) {
  uint8_t cmd[] = {0xFF};
  uint8_t resp_buf[2];

  write(pgm->fd, cmd, 1);
  if (!error_check(pgm->fd, cmd[0], resp_buf, 2)) return 0;

  int version = resp_buf[0] << 8 | resp_buf[1];
  if (version > 0) {
    set_error(ESPSTLINK_ERROR_VERSION, "Unsupported target version: %d.\n",
              version);
    error.device_code = version;
    return 0;
  }
  return 1;
}

bool espstlink_swim_entry(const espstlink_t *pgm) {
  uint8_t cmd[] = {0xFE};
  uint8_t resp_buf[2];

  write(pgm->fd, cmd, 1);
  if (!error_check(pgm->fd, cmd[0], resp_buf, 2)) return 0;

  int duration = resp_buf[0] << 8 | resp_buf[1];
  if (duration < 1200 || duration > 1360) {
    fprintf(stderr,
            "Warning: remote device took %d cycles (%d us) (expected: %d us)\n",
            duration, duration / 80, 128 / 8);
  }
  return 1;
}

bool espstlink_swim_srst(const espstlink_t *pgm) {
  uint8_t cmd[] = {0};

  write(pgm->fd, cmd, 1);
  return error_check(pgm->fd, cmd[0], NULL, 0);
}

bool espstlink_swim_read(const espstlink_t *pgm, uint8_t *buffer,
                         unsigned int addr, size_t size) {
  uint8_t cmd[] = {1, size, addr >> 16, addr >> 8, addr};
  uint8_t resp_buf[512];
  write(pgm->fd, cmd, 5);
  if (!error_check(pgm->fd, cmd[0], resp_buf, cmd[1] + 4)) return 0;
  // there's 4 non data bytes in the response: len, 3*address
  memcpy(buffer, resp_buf + 4, size);
  return 1;
}

bool espstlink_swim_write(const espstlink_t *pgm, const uint8_t *buffer,
                          unsigned int addr, size_t size) {
  uint8_t cmd[] = {2, size, addr >> 16, addr >> 8, addr};
  write(pgm->fd, cmd, 5);
  write(pgm->fd, buffer, cmd[1]);

  uint8_t resp_buf[4];
  return error_check(pgm->fd, cmd[0], resp_buf, 4);
}

void espstlink_close(espstlink_t *pgm) {
  close(pgm->fd);
  free(pgm);
}
