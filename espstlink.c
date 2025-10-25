/**
 * Copyright (C) 2017 Hagen Fritsch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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
  int version = pgm->espstlink->version;
  bool ret = false;

  // Enter reset state, if the programmer firmware supports this.
  if (version > 0 && !espstlink_reset(pgm->espstlink, /*input=*/0, 1)) return 0;

  if (!espstlink_swim_entry(pgm->espstlink)) return 0;

  if (!espstlink_swim_srst(pgm->espstlink)) return 0;
  usleep(1);
  ret = espstlink_write_byte(pgm, 0xA0, 0x7f80);  // Init the SWIM_CSR.

  // Put the reset pin back into pullup state.
  if (version > 0 && !espstlink_reset(pgm->espstlink, /*input=*/1, 0)) return 0;

  return ret;
}

// Set / Unsets the STALL bit in the DM_CSR2 register. Stops / Resumes the CPU.
static bool espstlink_stall(programmer_t *pgm, bool stall) {
  // Set the STALL bit in DM_CSR2, to stop any code from executing.
  int csr = espstlink_read_byte(pgm, DM_CSR2);
  if (csr == -1) return 0;
  return espstlink_write_byte(pgm, stall ? csr | 8 : csr & ~8, DM_CSR2);
}

static bool espstlink_prepare_for_flash(programmer_t *pgm,
                                        const stm8_device_t *device,
                                        const memtype_t memtype) {
  if (!espstlink_stall(pgm, true)) return 0;

  // Unlock MASS
  if (memtype == FLASH) {
    if (!espstlink_write_byte(pgm, 0x56, device->regs.FLASH_PUKR)) return 0;
    if (!espstlink_write_byte(pgm, 0xae, device->regs.FLASH_PUKR)) return 0;
  }
  if (memtype == EEPROM || memtype == OPT) {
    if (!espstlink_write_byte(pgm, 0xae, device->regs.FLASH_DUKR)) return 0;
    if (!espstlink_write_byte(pgm, 0x56, device->regs.FLASH_DUKR)) return 0;
  }

  return 1;
}

static void espstlink_wait_until_transfer_completes(
    programmer_t *pgm, const stm8_device_t *device) {
  // Wait for EOP to be set in FLASH_IAPSR
  // provide a better error message than 'tries exceeded'
  do {
    int retries = 5;
    int iapsr;
    while (retries > 0) {
      iapsr = espstlink_read_byte(pgm, device->regs.FLASH_IAPSR);
      if (iapsr & 0x04) break;
      if (iapsr & 0x01) {
          ERROR("target page is write protected (UBC) or read-out protection is enabled");
      }
      retries--;
      usleep(10000);
    }
  } while (0);
}

int espstlink_swim_read_range(programmer_t *pgm, const stm8_device_t *device,
                              unsigned char *buffer, unsigned int start,
                              unsigned int length) {
  size_t i = 0;
  for (; i < length;) {
    int current_size = length - i;
    if (current_size > 255)
      current_size = 255;
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
  size_t i = 0;

  espstlink_prepare_for_flash(pgm, device, memtype);

  if (memtype == OPT) {
    // Option programming mode
    espstlink_write_byte(pgm, 0x80, device->regs.FLASH_CR2);
    if (device->regs.FLASH_NCR2 != 0) {
      espstlink_write_byte(pgm, 0x7F, device->regs.FLASH_NCR2);
    }

    for (i = 0; i < length; i++) {
        espstlink_write_byte(pgm, *(buffer++), start++);
        // Wait for EOP to be set in FLASH_IAPSR
        usleep(6000); // t_prog per the datasheets is 6ms typ, 6.6ms max
        TRY(5,espstlink_read_byte(pgm, device->regs.FLASH_IAPSR) & 0x04);
    }
  } else {
    unsigned int rounded_size = ((length - 1) / device->flash_block_size + 1) * device->flash_block_size;
    unsigned char *current = alloca(rounded_size);

    // Read existing data
    espstlink_swim_read_range(pgm, device, current, start, rounded_size);

    // Extend the new data with the existing if it doesn't fill a complete flash block
    // (this is safe as the incoming buffer is actually as big as the device's flash,
    // not just the image to be flashed).
    memcpy(buffer + length, current + length, rounded_size - length);

    for (; i < length; i += device->flash_block_size) {
      // Write one block at a time.
      if (memcmp(current + i, buffer + i, device->flash_block_size)) {
        uint8_t prgmode = 0x10;

        if (memtype == FLASH || memtype == EEPROM) {
          /*
           * Use fast block programming (prgmode = 0x10) only if we have
           * read the flash block and verified that it is empty (all its
           * bytes are 0x00).
           */
          for (int j = 0; j < device->flash_block_size; j++) {
              if (current[i + j]) {
                  // Not empty so use standard block programming
                  prgmode = 0x01;
                  break;
              }
          }

          // Block programming mode
          espstlink_write_byte(pgm, prgmode, device->regs.FLASH_CR2);
          if(device->regs.FLASH_NCR2 != 0) {
            espstlink_write_byte(pgm, ~prgmode, device->regs.FLASH_NCR2);
          }
        }

        if (!espstlink_swim_write(pgm->espstlink, buffer + i, start + i,
                                  device->flash_block_size))
          return i;

        if (memtype == FLASH || memtype == EEPROM) {
          // t_prog per the datasheets is 6ms typ, 6.6ms max, fast mode is twice as fast
          usleep(prgmode == 0x10 ? 3000 : 6000);
          espstlink_wait_until_transfer_completes(pgm, device);
        }
      }
    }
  }

  if (memtype == FLASH || memtype == EEPROM || memtype == OPT) {
      // Reset DUL and PUL in IAPSR to disable flash and data writes.
      int iapsr = espstlink_read_byte(pgm, device->regs.FLASH_IAPSR);
      if (iapsr != -1)
        espstlink_write_byte(pgm, iapsr & (~0x0a), device->regs.FLASH_IAPSR);
  }

  return i;
}

void espstlink_srst(programmer_t *pgm) {
  espstlink_swim_srst(pgm->espstlink);
  espstlink_stall(pgm, false);
}

bool espstlink_pgm_open(programmer_t *pgm) {
  pgm->espstlink = espstlink_open(pgm->port);
  return pgm->espstlink != NULL && espstlink_fetch_version(pgm->espstlink) &&
         espstlink_swim_reconnect(pgm);
}

void espstlink_pgm_close(programmer_t *pgm) {
  espstlink_close(pgm->espstlink);
  pgm->espstlink = NULL;
}
