#include "byte_utils.h"

void format_int(unsigned char *out, unsigned int in, unsigned char length, unsigned char endianess) {
	int i, idx;
	for(i = 0; i < length; i++) {
		idx = endianess == MP_LITTLE_ENDIAN ? i : length - 1 - i;
		out[i] = (in & 0xFF << idx*8) >> idx*8;
	}
}

int load_int(unsigned char *buf, unsigned char length, unsigned char endianess) {
	int i, idx, result = 0;
	for(i = 0; i < length; i++) {
		idx = endianess == MP_LITTLE_ENDIAN ? i : length - 1 - i;
		result |= (buf[i] << idx*8);
	}
	return(result);
}
