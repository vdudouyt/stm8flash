/* stlink/v2 stm8 memory programming utility
   (c) Valentin Dudouyt, 2012 - 2014 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ihex.h"
#include "error.h"
#include "byte_utils.h"

char line[256];

static unsigned char checksum(char *buf, unsigned int length, int chunk_len, int chunk_addr, int chunk_type) {
	int sum = chunk_len + (LO(chunk_addr)) + (HI(chunk_addr)) + chunk_type;
	int i;
	for(i = 0; i < length; i++) {
		sum += buf[i];
	}

	int complement = (~sum + 1);
	return complement & 0xff;
}

int ihex_read(FILE *pFile, unsigned char *buf, unsigned int start, unsigned int end) {
	fseek(pFile, 0, SEEK_SET);
	unsigned int chunk_len, chunk_addr, chunk_type, i, byte, line_no = 0, greatest_addr = 0;
	while(fgets(line, sizeof(line), pFile)) {
		line_no++;
		// Reading chunk header
		if(sscanf(line, ":%02x%04x%02x", &chunk_len, &chunk_addr, &chunk_type) != 3) {
			free(buf);
			ERROR2("Error while parsing IHEX at line %d\n", line_no);
		}
		// Reading chunk data
		for(i = 9; i < strlen(line) - 1; i +=2) {
			if(sscanf(&(line[i]), "%02x", &byte) != 1) {
				free(buf);
				ERROR2("Error while parsing IHEX at line %d byte %d\n", line_no, i);
			}
			if(chunk_type != 0x00) {
				// The only data records have to be processed
				continue;
			}
			if((i - 9) / 2 >= chunk_len) {
				// Respect chunk_len and do not capture checksum as data
				break;
			}
			if(chunk_addr < start) {
				free(buf);
				ERROR2("Address %04x is out of range at line %d\n", chunk_addr, line_no);
			}
			if(chunk_addr + chunk_len > end) {
				free(buf);
				ERROR2("Address %04x + %d is out of range at line %d\n", chunk_addr, chunk_len, line_no);
			}
			if(chunk_addr + chunk_len > greatest_addr) {
				greatest_addr = chunk_addr + chunk_len;
			}
			buf[chunk_addr - start + (i - 9) / 2] = byte;
		}
	}

	return(greatest_addr - start);
}
