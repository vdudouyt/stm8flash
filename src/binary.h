#ifndef __BINARY_H
#define __BINARY_H

#include <stdio.h>

#include "region.h"

int32_t binary_read(FILE *fp, struct region **r);

int32_t binary_write(FILE *fp, struct region *r);

#endif
