#ifndef __BYTE_UTILS_H
#define __BYTE_UTILS_H

#define MP_LITTLE_ENDIAN 0
#define MP_BIG_ENDIAN 1

#define HI(x) (((x) & 0xff00) >> 8)
#define LO(x) ((x) & 0xff)

void format_int(unsigned char *out, unsigned int in, unsigned char length, unsigned char endianess);
int load_int(unsigned char *buf, unsigned char length, unsigned char endianess);

#endif
