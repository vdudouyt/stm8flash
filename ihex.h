#ifndef __IHEX_H
#define __IHEX_H

int ihex_read(FILE *pFile, unsigned char *buf, unsigned int start, unsigned int end);

#endif
