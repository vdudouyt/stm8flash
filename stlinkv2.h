#ifndef __STLINKV2_H
#define __STLINKV2_H

#include "pgm.h"

bool stlink2_open(programmer_t *pgm);
void stlink2_srst(programmer_t *pgm);
int stlink2_swim_read_range(programmer_t *pgm, const stm8_device_t *device, unsigned char *buffer, unsigned int start, unsigned int length);
int stlink2_swim_write_range(programmer_t *pgm, const stm8_device_t *device, unsigned char *buffer, unsigned int start, unsigned int length, const memtype_t memtype);

#endif
