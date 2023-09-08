/***************************************************************************
 *   Intel hex read and write utility functions                            *
 *                                                                         *
 *   Copyright (c) Valentin Dudouyt, 2004 2012 - 2014                      *
 *   Copyright (c) Philipp Klaus Krause, 2021                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "ihex.h"

#include <string.h>

#define HI(x) (((x)&0xff00) >> 8)
#define LO(x) ((x)&0xff)

static unsigned char checksum(unsigned char *buf, unsigned int length, int chunk_len, int chunk_addr, int chunk_type) {
	int sum = chunk_len + (LO(chunk_addr)) + (HI(chunk_addr)) + chunk_type;
	int i;
	for (i = 0; i < length; i++) {
		sum += buf[i];
	}

	int complement = (~sum + 1);
	return (complement & 0xff);
}

int ihex_read(FILE *pFile, struct region **r) {
	// a record in intel hex looks like
	// :LLXXXXTTDDDD....DDCC
	// Where LL is the length of the data field and can be as large as 255.  So the maximum size of a record is 9 characters for the
	// header, 255 * 2 characters for the data, 2 characters for the Checksum, and an additional 2 characters (possibly a Carriage
	// Return and linefeed.  This is then a total an 9 + 510 + 2 + 2 = 523.
	static char line[523];

	unsigned int chunk_len, chunk_addr, chunk_type, byte, line_no = 0;

	while (fgets(line, sizeof(line), pFile)) {
		line_no++;

		// Strip off Carriage Return at end of line if it exists.
		if (line[strlen(line)] == '\r') {
			line[strlen(line)] = 0;
		}

		// Reading chunk header
		if (sscanf(line, ":%02x%04x%02x", &chunk_len, &chunk_addr, &chunk_type) != 3) {
			fprintf(stderr, "Error while parsing IHEX at line %d\n", line_no);
			return (-1);
		}
		// Reading chunk data
		// Extended Segment Address
		if (chunk_type == 2) {
			unsigned int esa;
			if (sscanf(&line[9], "%04x", &esa) != 1) {
				fprintf(stderr, "Error while parsing IHEX at line %d\n", line_no);
				return (-1);
			}
		}
		// Extended Linear Address
		if (chunk_type == 4) {
			unsigned int ela;
			if (sscanf(&line[9], "%04x", &ela) != 1) {
				fprintf(stderr, "Error while parsing IHEX at line %d\n", line_no);
				return (-1);
			}
		}

		for (int i = 9; i < strlen(line) - 1; i += 2) {
			if (sscanf(&(line[i]), "%02x", &byte) != 1) {
				fprintf(stderr, "Error while parsing IHEX at line %d byte %d\n", line_no, i);
				return (-1);
			}
			if (chunk_type != 0x00) {
				// The only data records have to be processed
				break;
			}
			if ((i - 9) / 2 >= chunk_len) {
				// Respect chunk_len and do not capture checksum as data
				break;
			}

			const uint8_t u8 = byte;
			if (region_add_data(r, chunk_addr + (i - 9) / 2, &u8, 1)) {
				fprintf(stderr, "Failed to add data to region at line %d\n", line_no);
				return -1;
			}
		}
	}

	return 0;
}

static inline uint32_t min(uint32_t a, uint32_t b) { return a < b ? a : b; }

int ihex_write(FILE *fp, struct region *r) {
	for (; r; r = r->next) {
		// If not, write an ELA record
		uint8_t ela_bytes[2];
		uint32_t cur_ela = r->start >> 16;
		ela_bytes[0] = HI(cur_ela);
		ela_bytes[1] = LO(cur_ela);
		if (fprintf(fp, ":02000004%04X%02X\n", cur_ela, checksum(ela_bytes, 2, 2, 0, 4)) < 0) {
			fprintf(stderr, "I/O error during IHEX write\n");
			return (-1);
		}

		for (uint32_t start = r->start; start < r->end;) {
			uint32_t chunk_len = min(16, r->end - start);
			fprintf(fp, ":%02X%04X00", chunk_len, start & 0xffff);
			for (uint32_t i = start; i < start + chunk_len; i++) {
				if (fprintf(fp, "%02X", r->b[i - r->start]) < 0) {
					fprintf(stderr, "I/O error during IHEX write\n");
					return (-1);
				}
			}
			if (fprintf(fp, "%02X\n", checksum(&r->b[start - r->start], chunk_len, chunk_len, start, 0)) < 0) {
				fprintf(stderr, "I/O error during IHEX write\n");
				return (-1);
			}

			start += chunk_len;
		}
	}

	// Add the END record
	if (fprintf(fp, ":00000001FF\n") < 0) {
		fprintf(stderr, "I/O error during IHEX write\n");
		return (-1);
	}

	return (0);
}
