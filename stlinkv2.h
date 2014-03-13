#ifndef __STLINKV2_H
#define __STLINKV2_H

unsigned int stlink2_get_status(programmer_t *pgm);
bool stlink2_open(programmer_t *pgm);
int stlink2_write_byte(programmer_t *pgm, unsigned char byte, unsigned int start);
int stlink2_write_and_read_byte(programmer_t *pgm, unsigned char byte, unsigned int start);
int stlink2_swim_read_range(programmer_t *pgm, stm8_device_t *device, char *buffer, unsigned int start, unsigned int length);
int stlink2_swim_write_range(programmer_t *pgm, stm8_device_t *device, char *buffer, unsigned int start, unsigned int length);

#endif
