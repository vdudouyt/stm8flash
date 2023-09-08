#ifndef __SREC_H
#define __SREC_H

#include <stdio.h>

#include "region.h"

int32_t srec_read(FILE *fp, struct region **r);

int32_t srec_write(FILE *fp, struct region *r);

#endif
