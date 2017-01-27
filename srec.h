#ifndef __SREC_H
#define __SREC_H

int srec_read(FILE *pFile, unsigned char *buf, unsigned int start, unsigned int end);

void srec_write(FILE *pFile, unsigned char *buf, unsigned int start, unsigned int end);

#endif
