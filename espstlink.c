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

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include "pgm.h"
#include "try.h"

#define DM_CSR2 0x7F99

static bool init(programmer_t *pgm) {
  struct termios tty;
  struct termios tty_old;
  memset(&tty, 0, sizeof tty);

  pgm->dev_fd = open(pgm->port ? pgm->port : "/dev/ttyUSB0", O_RDWR | O_NOCTTY);
  if (pgm->dev_fd < 0) {
    perror("Couldn't open tty");
    return 0;
  }

  /* Error Handling */
  if (tcgetattr(pgm->dev_fd, &tty) != 0) {
    perror("Couldn't open tty");
    return 0;
  }

  /* Save old tty parameters */
  tty_old = tty;

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
  tcflush(pgm->dev_fd, TCIFLUSH);
  if (tcsetattr(pgm->dev_fd, TCSANOW, &tty) != 0) {
    perror("Setting tty attributes failed");
    return 0;
  }
  return 1;
}

static bool error_check(int fd, uint8_t command, uint8_t *resp_buf,
                        size_t size) {
  uint8_t buf[4];
  int len = read(fd, buf, 1);
  if (len < 1) {
    fprintf(stderr, "Didn't get a response from the device: %s\n",
            strerror(errno));
    return 0;
  }
  if (buf[0] != command) {
    fprintf(stderr, "Unexpected data: %02x\n", buf[0]);
    return 0;
  }
  len = read(fd, buf + 1, 1);
  if (len < 1) {
    fprintf(stderr, "Device didn't finish command 0x%02x: %s\n", buf[0],
            strerror(errno));
    return 0;
  }
  if (buf[1] == 0) {
    if (resp_buf && size) {
      size_t total = 0;
      while (total < size) {
        len = read(fd, resp_buf + total, size);
        if (len < 1) {
          fprintf(stderr,
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
      fprintf(
          stderr,
          "Device didn't finish sending error code for command 0x%02x: %s\n",
          buf[0], strerror(errno));
      return 0;
    }
    int code = buf[2] << 8 | buf[3];
    fprintf(stderr, "Command 0x%02x failed with code: 0x%02x\n", buf[0], code);
  } else {
    fprintf(stderr, "Unexpected error code for command 0x%02x: 0x%02x\n",
            buf[0], buf[1]);
  }
  return 0;
}

static bool check_version(programmer_t *pgm) {
  uint8_t cmd[] = {0xFF};
  uint8_t resp_buf[2];

  write(pgm->dev_fd, cmd, 1);
  if (!error_check(pgm->dev_fd, cmd[0], resp_buf, 2)) return 0;

  int version = resp_buf[0] << 8 | resp_buf[1];
  if (version > 0) {
    fprintf(stderr, "Unsupported target version: %d.\n", version);
    return 0;
  }
  return 1;
}

static bool swim_entry(programmer_t *pgm) {
  uint8_t cmd[] = {0xFE};
  uint8_t resp_buf[2];

  write(pgm->dev_fd, cmd, 1);
  if (!error_check(pgm->dev_fd, cmd[0], resp_buf, 2)) return 0;

  int duration = resp_buf[0] << 8 | resp_buf[1];
  if (duration < 1200 || duration > 1360) {
    fprintf(stderr,
            "Warning: remote device took %d cycles (%d ms) (expected: %d ms)\n",
            duration, duration / 80, 128 / 8);
  }
  return 1;
}

static bool swim_srst(programmer_t *pgm) {
  uint8_t cmd[] = {0};
  uint8_t resp_buf[4];

  write(pgm->dev_fd, cmd, 1);
  return error_check(pgm->dev_fd, cmd[0], NULL, 0);
}

static bool espstlink_swim_read(programmer_t *pgm, uint8_t *buffer,
                                unsigned int addr, size_t size) {
  uint8_t cmd[] = {1, size, addr >> 16, addr >> 8, addr};
  uint8_t resp_buf[512];
  write(pgm->dev_fd, cmd, 5);
  if (!error_check(pgm->dev_fd, cmd[0], resp_buf, cmd[1] + 4)) return 0;
  // there's 4 non data bytes in the response: len, 3*address
  memcpy(buffer, resp_buf + 4, size);
  return 1;
}

static bool espstlink_swim_write(programmer_t *pgm, const uint8_t *buffer,
                                 unsigned int addr, size_t size) {
  uint8_t cmd[] = {2, size, addr >> 16, addr >> 8, addr};
  write(pgm->dev_fd, cmd, 5);
  write(pgm->dev_fd, buffer, cmd[1]);

  uint8_t resp_buf[4];
  return error_check(pgm->dev_fd, cmd[0], resp_buf, 4);
}

static int espstlink_read_byte(programmer_t *pgm, unsigned int addr) {
  uint8_t byte;
  if (!espstlink_swim_read(pgm, &byte, addr, 1)) return -1;
  return byte;
}

static bool espstlink_write_byte(programmer_t *pgm, uint8_t byte,
                                 unsigned int addr) {
  return espstlink_swim_write(pgm, &byte, addr, 1);
}

static bool espstlink_swim_reconnect(programmer_t *pgm) {
  if (!swim_entry(pgm)) return 0;
  if (!swim_srst(pgm)) return 0;
  usleep(1);
  return espstlink_write_byte(pgm, 0xA0, 0x7f80);  // Init the SWIM_CSR.
}

static bool espstlink_prepare_for_flash(programmer_t *pgm,
                                        const stm8_device_t *device,
                                        const memtype_t memtype) {
  // Set the STALL bit in DM_CSR2, to step any code from executing.
  uint8_t csr = espstlink_read_byte(pgm, DM_CSR2);
  espstlink_write_byte(pgm, csr | 8, DM_CSR2);

  // Unlock MASS
  if (memtype == FLASH) {
    espstlink_write_byte(pgm, 0x56, device->regs.FLASH_PUKR);
    espstlink_write_byte(pgm, 0xae, device->regs.FLASH_PUKR);
  }
  if (memtype == EEPROM || memtype == OPT) {
    espstlink_write_byte(pgm, 0xae, device->regs.FLASH_DUKR);
    espstlink_write_byte(pgm, 0x56, device->regs.FLASH_DUKR);
  }

  // Set the PRG bit in FLASH_CR2 and reset it in FLASH_NCR2.
  uint8_t mode = 0x01;
  uint8_t flash_cr2[] = {mode, ~mode};
  espstlink_swim_write(pgm, flash_cr2, device->regs.FLASH_CR2, 2);
}

static void espstlink_wait_until_transfer_completes(
    programmer_t *pgm, const stm8_device_t *device) {
  // wait until the EOP bit is set.
  TRY(8, espstlink_read_byte(pgm, device->regs.FLASH_IAPSR) & 0x4);
}

int espstlink_swim_read_range(programmer_t *pgm, const stm8_device_t *device,
                              unsigned char *buffer, unsigned int start,
                              unsigned int length) {
  espstlink_swim_reconnect(pgm);

  size_t i = 0;
  for (; i < length;) {
    int current_size = MIN(length - i, 255);
    if (!espstlink_swim_read(pgm, buffer + i, start + i, current_size))
      return i;
    i += current_size;
  }
  return i;
}

int espstlink_swim_write_range(programmer_t *pgm, const stm8_device_t *device,
                               unsigned char *buffer, unsigned int start,
                               unsigned int length, const memtype_t memtype) {
  espstlink_swim_reconnect(pgm);
  espstlink_prepare_for_flash(pgm, device, memtype);

  size_t i = 0;
  for (; i < length;) {
    // Write one block (128 bytes) at a time.
    int current_size = MIN(length - i, 128);
    if (!espstlink_swim_write(pgm, buffer + i, start + i, current_size))
      return i;
    i += current_size;

    espstlink_wait_until_transfer_completes(pgm, device);
    // TODO: Check the WR_PG_DIS bit in FLASH_IAPSR to verify if the block you
    // attempted to program was not write protected (optional)
  }
  return i;
}

void espstlink_srst(programmer_t *pgm) { swim_srst(pgm); }

bool espstlink_open(programmer_t *pgm) {
  return init(pgm) && check_version(pgm) && espstlink_swim_reconnect(pgm);
}

void espstlink_close(programmer_t *pgm) { close(pgm->dev_fd); }
