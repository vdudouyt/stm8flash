/* stlink/v2 stm8 memory programming utility
	 (c) Valentin Dudouyt, 2012 - 2014 */

#include "srec.h"

#include <string.h>

// convert a hex encoded string of dstlen*2 to raw bytes
// returns the number of bytes successfully decoded
int32_t hex2bin(uint8_t *dst, int32_t dstlen, char *src) {
	int32_t count = 0;
	for (; dstlen > 0; dstlen -= 1) {
		const char hi = src[0];
		const char lo = src[1];

		uint8_t x = 0;
		if (hi >= 48 && hi <= 57) {
			x = (uint8_t)(hi)-48;
		} else if (hi >= 65 && hi <= 70) {
			x = (uint8_t)(hi)-65 + 10;
		} else if (hi >= 97 && hi <= 102) {
			x = (uint8_t)(hi)-97 + 10;
		} else {
			return count;
		}

		uint8_t y = 0;
		if (lo >= 48 && lo <= 57) {
			y = (uint8_t)(lo)-48;
		} else if (lo >= 65 && lo <= 70) {
			y = (uint8_t)(lo)-65 + 10;
		} else if (lo >= 97 && lo <= 102) {
			y = (uint8_t)(lo)-97 + 10;
		} else {
			return count;
		}

		*dst = (x << 4) | y;

		dst += 1;
		src += 2;
		count += 1;
	}
	return count;
}

// modified fgets to read complete lines at a time.
// at most dst_len-1 bytes are coppied from file up to and including the newline character. reading stops at EOF or newline.;
char *fgetss(char *const org, const uint32_t dst_len, FILE *const fp) {
	int32_t c = 0;
	char *dst = org;
	const char *const end = dst + dst_len - 1;

	do {
		c = fgetc(fp);
		if (c == EOF) {
			break;
		}

		if (dst < end) {
			*dst = c;
			dst += 1;
			*dst = '\0';
		}
	} while ((c != EOF) && (c != '\n'));

	if (org < dst) {
		return org;
	} else {
		return NULL;
	}
}

// parse the srec data from file `fp` into `data`; a buffer where data[0] corresponds to the byte at address `startaddr`
int32_t srec_read(FILE *const fp, struct region **r) {
	// A SRECORD has one of the following formats
	// SXLLAAAADDDD....DDCC
	// SXLLAAAAAADDDD....DDCC
	// SXLLAAAAAAAADDDD....DDCC
	// Where S is the letter 'S', X is a single digit indicating the record type, LL is the length of the
	// record from the start of the address field through the checksum (i.e. the length of the rest of the
	// record), AAAA[AA[AA]] is the address field and can be 16, 24 or 32 bits depending on the record type,
	// DD is the data for the record (and optional for an S0 record, and not present in some other records),
	// anc CC is the ones complement chechsum of the bytes starting at LL up to the last DD byte.
	// The maximum value for LL is 0xFF, indication a length from the first AA byte through CC byte of 255.
	// The maximum total length in characters would then be 4 (for the SXLL characters) plus 255 * 2 plus a
	// possible carriage return and line feed.  This would then be 4+510+2, or 516 characters.
	static char line[516];
	static uint8_t bin[sizeof(line) / 2];
	int32_t line_number = 0;
	while (fgetss(line, sizeof(line), fp)) {
		line_number += 1;

		// header and type
		if (line[0] != 'S') {
			// not an S record
			continue;
		}

		const char record_type = line[1];
		if ((record_type == '4') || (record_type > '9')) {
			fprintf(stderr, "line %d: record_type %d is not supported\n", line_number, record_type);
			return -1;
		}

		// decode
		const int32_t srclen = strlen(line);
		const int32_t dstlen = (srclen - 2) / 2;
		const int32_t count = hex2bin(bin, dstlen, &line[2]);
		if (count < 1) {
			fprintf(stderr, "line %d: bad srec record\n", line_number);
			return -1;
		}

		// check record length
		const int32_t record_len = bin[0];
		if (record_len > count) {
			fprintf(stderr, "line %d: bad srec record\n", line_number);
			return -1;
		}

		// check csum
		uint8_t csum = 0;
		for (int32_t i = 0; i < count - 1; i++) {
			csum += bin[i];
		}
		csum = 0xFF - csum;
		if (csum != bin[count - 1]) {
			fprintf(stderr, "line %d: csum: %02X != %02X\n", line_number, csum, bin[count - 1]);
			return -1;
		}

		// find address and actual data to copy
		uint8_t *data_src = bin + 1;								// strip length byte
		uint8_t *const data_end = bin + record_len; // points to checksum
		uint32_t data_addr;

		if (record_type == '0') {
			// header
			data_src = NULL;
		} else if (record_type == '1') {
			// data 16 bit addr
			data_addr = ((uint32_t)data_src[0] << 8) | data_src[1];
			data_src += 2; // skip addr
		} else if (record_type == '2') {
			// data 24 bit addr
			data_addr = ((uint32_t)data_src[0] << 16) | ((uint32_t)data_src[1] << 8) | data_src[2];
			data_src += 3; // skip addr
		} else if (record_type == '3') {
			// data 32 bit addr
			data_addr = ((uint32_t)data_src[0] << 24) | ((uint32_t)data_src[1] << 16) | ((uint32_t)data_src[2] << 8) | data_src[3];
			data_src += 4; // skip addr
		} else if (record_type == '4') {
			// reserved
			data_src = NULL;
		} else if (record_type == '5') {
			// count in 16 bits
			data_src = NULL;
		} else if (record_type == '6') {
			// count in 24 bits
			data_src = NULL;
		} else if (record_type == '7') {
			// execution addr 32 bit
			data_src = NULL;
		} else if (record_type == '8') {
			// execution addr 24 bit
			data_src = NULL;
		} else if (record_type == '9') {
			// execution addr 16 bit
			data_src = NULL;
		} else {
			data_src = NULL;
		}

		// copy
		if (data_src) {
			if (data_src >= data_end) {
				fprintf(stderr, "bad record %d\n", line_number);
				return -1;
			}

			if (region_add_data(r, data_addr, data_src, data_end - data_src)) {
				fprintf(stderr, "unable to load data on line %d\n", line_number);
				return -1;
			}
		}
	}

	return 0;
}

/*
 * Checksum for SRECORD.  Add all the bytes from the record length field through the data, and take
 * the ones complement.  This includes the address field, which for SRECORDs can be 2 bytes, 3 bytes or
 * 4 bytes.  In this case, since the upper bytes of the address will be 0 for records of the smaller sizes,
 * just add all 4 bytes of the address in all cases.
 *
 * The arguments are:
 *
 *  buf		a pointer to the beginning of the data for the record
 *  length	the length of the data portion of the record
 *  chunk_len	the length of the record starting at the address and including the checksum
 *  chunk_addr	starting address for the data in the record
 *
 * Returns an uint8_t with the checksum
 */
static uint8_t srec_csum(uint8_t *buf, uint32_t length, uint32_t chunk_len, uint32_t chunk_addr) {
	uint32_t sum = chunk_len;

	for (uint32_t i = 0; i < sizeof(chunk_addr); i++) {
		sum += ((uint8_t *)&chunk_addr)[i];
	}

	for (uint32_t i = 0; i < length; i++) {
		sum += buf[i];
	}

	return ~sum & 0xff;
}

static uint32_t min(uint32_t a, uint32_t b) { return a < b ? a : b; }

int32_t srec_write(FILE *fp, struct region *r) {
	for (; r; r = r->next) {
		for (uint32_t start = r->start; start < r->end;) {
			const uint8_t dlen = min(16, r->end - start);
			const uint8_t len = dlen + 4 + 1;
			fprintf(fp, "S3%02X%08X", len, start);
			for (int i = 0; i < dlen; i++) {
				fprintf(fp, "%02X", r->b[start - r->start + i]);
			}
			fprintf(fp, "%02X\n", srec_csum(&r->b[start - r->start], dlen, len, start));
			start += dlen;
		}
	}

	/*    // Add S5 record, for count of data records
			fprintf(pFile, "S503%04X", num_records & 0xffff);
			fprintf(pFile, "%02X\n", srec_csum(buf, 0, 3, num_records & 0xffff));
			// Add End record. S9 terminates S1, S8 terminates S2 and S7 terminates S3 records
			fprintf(pFile, "S%1X%02X%0*X",10-data_rectype, addr_width/2 + 1, addr_width, 0);
			fprintf(pFile, "%02X\n", srec_csum(buf, 0, addr_width/2 + 1, 0));*/

	return 0;
}
