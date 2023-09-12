#include "region.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline uint32_t max(uint32_t a, uint32_t b) { return a > b ? a : b; }

static inline uint32_t min(uint32_t a, uint32_t b) { return a < b ? a : b; }

void region_shift(struct region *r, uint32_t offx) {
	for (;r; r = r->next) {
		r->start += offx;
	}
}

static int region_append(struct region *r, const uint8_t *b, uint32_t blen) {
	const uint32_t newlen = r->end - r->start + blen;
	if (newlen > r->blen) {
		const uint32_t newblen = max(r->blen * 2, newlen);
		uint8_t *const newb = realloc(r->b, newblen);
		if (!newb) {
			return -1;
		}

		r->b = newb;
		r->blen = newblen;
	}

	memcpy(&r->b[r->end - r->start], b, blen);
	r->end += blen;
	return 0;
}

static struct region *new_region(uint32_t start, const uint8_t *b, uint32_t blen) {
	struct region *const r = malloc(sizeof(*r));
	if (!r) {
		return NULL;
	}

	r->b = malloc(blen);
	if (!r->b) {
		return NULL;
	}

	memcpy(r->b, b, blen);

	r->start = start;
	r->blen = blen;
	r->end = r->start + blen;
	r->next = NULL;
	return r;
}

int region_add_data(struct region **r, uint32_t start, const uint8_t *b, uint32_t blen) {
	// fprintf(stderr, "add data: %08X %d\n", start, blen);
	// uint32_t end = start + blen;
	for (; *r; r = &(*r)->next) {
		if ((*r)->end == start) {
			// append here
			// uint32_t newend = (*r)->end + blen;

			const uint32_t availible = ((*r)->next) ? (*r)->next->start - (*r)->end : blen;

			const uint32_t copylen = min(availible, blen);

			if (region_append(*r, b, copylen)) {
				return -1;
			}

			b += copylen;
			blen -= copylen;
			start += copylen;
		} else if ((*r)->end >= start && start >= (*r)->start) {
			ERR("overlapping regions detected! [0x%08X:0x%08X] clashes with [0x%08X:0x%08X]", start, start+blen, (*r)->end, (*r)->start);
			return -1;
		}
		if (blen == 0) {
			break;
		}
	}

	if (blen) {
		*r = new_region(start, b, blen);
		if (!*r) {
			return -1;
		}
	}

	return 0;
}

int region_add_empty(struct region **r, uint32_t start, uint32_t blen) {
	uint8_t *const data = malloc(blen); // FIXME: remove
	if (!data) {
		ERR("malloc");
		return -1;
	}

	memset(data, 0, blen);

	if (region_add_data(r, start, data, blen)) {
		free(data);
	}

	return 0;
}

int region_get_data(struct region *r, uint32_t start, uint8_t *b, uint32_t blen) {
	for (; r; r = r->next) {
		if (r->start <= start && start <= r->end) {
			const uint32_t availible = r->end - start;

			const uint32_t copylen = min(availible, blen);

			memcpy(b, &r->b[start - r->start], copylen);

			b += copylen;
			blen -= copylen;
			start += copylen;
		}
	}

	if (blen) {
		return -1;
	}

	return 0;
}

void region_free(struct region **r) {
	while (*r) {
		free((*r)->b);
		struct region *const tmp = *r;
		*r = (*r)->next;
		free(tmp);
	}
}

struct region *region_intersection(struct region *dst, struct region *src) {
	struct region *r = NULL;
	// make sure src is contained in of dst
	for (struct region *s = src; s; s = s->next) {
		uint32_t start = s->start;
		while (start < s->end) {
			int found = 0;
			for (struct region *d = dst; d; d = d->next) {
				if (d->start <= start && start < d->end) {
					const uint32_t copylen = min(d->end - start, s->end - start);
					// create new region
					if (region_add_data(&r, start, &s->b[start - s->start], copylen)) {
						ERR("malloc");
						region_free(&r);
						return NULL;
					}
					start += copylen;
					found = 1;
					break;
				}
			}

			if (!found) {
				break;
			}
		}
	}
	return r;
}
