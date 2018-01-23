#ifndef __ESPSTLINKV_H
#define __ESPSTLINKV_H

#include <stdbool.h>
#include "pgm.h"

int espstlink_swim_read_range(programmer_t *pgm, const stm8_device_t *device,
                              unsigned char *buffer, unsigned int start,
                              unsigned int length);
int espstlink_swim_write_range(programmer_t *pgm, const stm8_device_t *device,
                               unsigned char *buffer, unsigned int start,
                               unsigned int length, const memtype_t memtype);
void espstlink_srst(programmer_t *pgm);
bool espstlink_pgm_open(programmer_t *pgm);
void espstlink_pgm_close(programmer_t *pgm);

#endif
