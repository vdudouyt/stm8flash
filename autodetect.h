#ifndef __AUTODETECT_H
#define __AUTODETECT_H

#include "pgm.h"

// return 0 = success, -1 = ROP active, -2 no match, -3 conflicting matches found, -4 comms error
int autodetect(programmer_t *pgm, stm8_device_t *autodetect_part);

#endif

