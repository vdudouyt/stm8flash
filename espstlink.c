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
#include "libespstlink.h"
#include "pgm.h"
#include "try.h"

#define DM_CSR2 0x7F99

static int espstlink_read_byte(programmer_t *pgm, unsigned int addr) {
  uint8_t byte;
  if (!espstlink_swim_read(pgm->espstlink, &byte, addr, 1)) return -1;
  return byte;
}

static bool espstlink_write_byte(programmer_t *pgm, uint8_t byte,
                                 unsigned int addr) {
  return espstlink_swim_write(pgm->espstlink, &byte, addr, 1);
}

static bool espstlink_swim_reconnect(programmer_t *pgm) {
  if (!espstlink_swim_entry(pgm->espstlink)) return 0;
  if (!espstlink_swim_srst(pgm->espstlink)) return 0;
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
  espstlink_swim_write(pgm->espstlink, flash_cr2, device->regs.FLASH_CR2, 2);
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
    if (!espstlink_swim_read(pgm->espstlink, buffer + i, start + i,
                             current_size))
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
    if (!espstlink_swim_write(pgm->espstlink, buffer + i, start + i,
                              current_size))
      return i;
    i += current_size;

    espstlink_wait_until_transfer_completes(pgm, device);
    // TODO: Check the WR_PG_DIS bit in FLASH_IAPSR to verify if the block you
    // attempted to program was not write protected (optional)
  }
  return i;
}

void espstlink_srst(programmer_t *pgm) { espstlink_swim_srst(pgm->espstlink); }

bool espstlink_pgm_open(programmer_t *pgm) {
  pgm->espstlink = espstlink_open(pgm->port);
  return pgm->espstlink != NULL && espstlink_check_version(pgm->espstlink) &&
         espstlink_swim_reconnect(pgm);
}

void espstlink_pgm_close(programmer_t *pgm) {
  espstlink_close(pgm->espstlink);
  pgm->espstlink = NULL;
}
