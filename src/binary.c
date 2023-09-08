
#include "binary.h"

int32_t binary_write(FILE *fp, struct region *r) {
	for (; r; r = r->next) {
		if (r->next && r->end != r->next->start) {
			// binary files must not contain gaps
			return -1;
		}
		const uint32_t dlen = r->end - r->start;
		if (fwrite(r->b, 1, dlen, fp) != dlen) {
			// failed to write
			return -1;
		}
	}

	return 0;
}


int32_t binary_read(FILE *fp, struct region **r) {
	uint32_t addr = 0;
	uint8_t tmp[1024*4];
	while (!feof(fp)) {
		size_t blen = fread(tmp, 1, sizeof(tmp), fp);
		
		if (blen) {
			if (region_add_data(r, addr, tmp, blen)) {
				return -1;
			}
		}
		
		if (ferror(fp)) {
			return -1;
		}
	}

	return 0;
}
