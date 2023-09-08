#ifndef __REGION_H
#define __REGION_H

#include <stdint.h>

struct region {
	struct region *next;
	uint32_t start;
	uint32_t end;
	uint32_t blen;
	uint8_t *b;
};

void region_shift(struct region *r, uint32_t offx);
int32_t region_add_empty(struct region **r, uint32_t start, uint32_t blen);
int32_t region_add_data(struct region **r, uint32_t start, const uint8_t *b, uint32_t blen);
int32_t region_get_data(struct region *r, uint32_t start, uint8_t *b, uint32_t blen);
void region_free(struct region **r);
struct region *region_intersection(struct region *a, struct region *b);

#endif //__REGION_H
