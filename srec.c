/* stlink/v2 stm8 memory programming utility
   (c) Valentin Dudouyt, 2012 - 2014 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "srec.h"
#include "error.h"
#include "byte_utils.h"

// convert a hex encoded string of dstlen*2 to raw bytes
// returns the number of bytes successfully decoded
int hex2bin(unsigned char *dst, int dstlen, char *src) {
	int count = 0;
	for (;dstlen > 0; dstlen -= 1) {
		const char hi = src[0];
		const char lo = src[1];

		unsigned char x = 0;
		if (hi >= 48 && hi <= 57) {
			x = (unsigned char)(hi) - 48;
		} else if (hi >= 65 && hi <= 70) {
			x = (unsigned char)(hi) - 65 + 10;
		} else if (hi >= 97 && hi <= 102) {
			x = (unsigned char)(hi) - 97 + 10;
		} else {
			return count;
		}
		
		unsigned char y = 0;
		if (lo >= 48 && lo <= 57) {
			y = (unsigned char)(lo) - 48;
		} else if (lo >= 65 && lo <= 70) {
			y = (unsigned char)(lo) - 65 + 10;
		} else if (lo >= 97 && lo <= 102) {
			y = (unsigned char)(lo) - 97 + 10;
		} else {
			return count;
		}
		
		*dst = (x<<4) | y;

		dst += 1;
		src += 2;
		count += 1;
	}
	return count;
}

// modified fgets to read complete lines at a time.
// at most dst_len-1 bytes are coppied from file up to and including the newline character. reading stops at EOF or newline.;
char *fgetss(char *const org, const unsigned int dst_len, FILE *const fp) {
	int c = 0;
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
int load_srec(unsigned char *const data, const unsigned int data_len, const unsigned int startaddr, FILE *const fp) {
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
	const unsigned int addrmax = startaddr + data_len;
	static char line[516];
	static unsigned char bin[sizeof(line)/2];
	int line_number = 0;
	unsigned int greatest_addr = 0;
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
			return 0;
		}
		
		// decode
		const int srclen = strlen(line);		
		const int dstlen = (srclen-2)/2;
		const int count = hex2bin(bin, dstlen, &line[2]);
		if (count < 1) {
			fprintf(stderr, "line %d: bad srec record\n", line_number);
			return 0;
		}
		
		// check record length
		const int record_len = bin[0];
		if (record_len > count) {
			fprintf(stderr, "line %d: bad srec record\n", line_number);
			return 0;
		}
		
		// check csum
		unsigned char csum = 0;
		for (int i = 0; i < count - 1; i++) {
			csum += bin[i];
		}
		csum = 0xFF - csum;
		if (csum != bin[count - 1]) {
			fprintf(stderr, "line %d: csum: %02X != %02X\n", line_number, csum, bin[count - 1]);
			return 0;
		}
		
		// find address and actual data to copy
		unsigned char *data_src = bin + 1; // strip length byte
		unsigned char *const data_end = bin + record_len; // points to checksum
		unsigned int data_addr;
		
		if (record_type == '0') {
			// header
			data_src = NULL;
		} else if (record_type == '1') {
			// data 16 bit addr
			data_addr = ((unsigned int)data_src[0]<<8) | data_src[1];
			data_src += 2; // skip addr
		} else if (record_type == '2') {
			// data 24 bit addr
			data_addr = ((unsigned int)data_src[0]<<16) | ((unsigned int)data_src[1]<<8) | data_src[2];
			data_src += 3; // skip addr
		} else if (record_type == '3') {
			// data 32 bit addr
			data_addr = ((unsigned int)data_src[0]<<24) | ((unsigned int)data_src[1]<<16) | ((unsigned int)data_src[2]<<8) | data_src[3];
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
		}

		// copy
		if (data_src) {
			if (data_src >= data_end) {
				fprintf(stderr, "bad record %d\n", line_number);
				return 0;
			}
			
			if (startaddr > data_addr) {
				fprintf(stderr, "address out of range on line %d\n", line_number);
				return 0;
			}
			
			const unsigned int copy_len = data_end - data_src;
			if (data_addr + copy_len >= addrmax) {
				fprintf(stderr, "address out of range on line %d\n", line_number);
				return 0;
			}

			unsigned char *data_dst = &data[data_addr - startaddr];
			memcpy(data_dst, data_src, copy_len);
			
			if (data_addr + copy_len > greatest_addr) {
				greatest_addr = data_addr + copy_len;
			}
		}
	}
	
	return greatest_addr - startaddr;
}

int srec_read(FILE *fp, unsigned char *buf, unsigned int start, unsigned int end) {
	if (end < start) {
		return 0;
	}
	
	fseek(fp, 0, SEEK_SET);
	const unsigned int data_len = end - start;
	const unsigned int data_loaded = load_srec(buf, data_len, start, fp);
	return data_loaded;
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
 * Returns an unsigned char with the checksum
 */
static unsigned char srec_csum(unsigned char *buf, unsigned int length, int chunk_len, int chunk_addr) {
  int sum = chunk_len + (LO(chunk_addr)) + (HI(chunk_addr)) + (EX(chunk_addr)) + (EH(chunk_addr));
  unsigned int i;
  for(i = 0; i < length; i++) {
    sum += buf[i];
  }
  return ~sum & 0xff;
}

void srec_write(FILE *pFile, unsigned char *buf, unsigned int start, unsigned int end) {
    int data_rectype = 1;  // Use S1 records if possible
    unsigned int chunk_len, chunk_start, i, addr_width;
    unsigned int num_records = 0;

    addr_width = 4;
    if(end >= 1<<16)
    {
	if(end >= 1<<24)
	{
	    data_rectype = 3; // addresses greater than 24 bit, use S3 records
	    addr_width = 8;
	}
	else
	{
	    data_rectype = 2; // addresses greater than 16 bits, but not greater than 24 bits, use S2 records
	    addr_width = 6;
	}
    }
    fprintf(pFile, "S0030000FC\n"); // Start with an S0 record
    chunk_start = start;
    while(chunk_start < end) {
       // Assume the rest of the bytes will be written in this chunk
	chunk_len = end - chunk_start;
	// Limit the length to 32 bytes per chunk
	if (chunk_len > 32)
	{
		chunk_len = 32;
	}
        // Write the data record
        fprintf(pFile, "S%1X%02X%0*X",data_rectype,chunk_len + addr_width/2 + 1,addr_width,chunk_start);
        for(i = chunk_start - start; i < (chunk_start + chunk_len - start); i++)
        {
                fprintf(pFile, "%02X",buf[i]);
        }
        fprintf(pFile, "%02X\n", srec_csum( &buf[chunk_start - start], chunk_len, chunk_len + addr_width/2 + 1, chunk_start));
	num_records++;
	chunk_start += chunk_len;
    }
    
    // Add S5 record, for count of data records
    fprintf(pFile, "S503%04X", num_records & 0xffff);
    fprintf(pFile, "%02X\n", srec_csum(buf, 0, 3, num_records & 0xffff));
    // Add End record. S9 terminates S1, S8 terminates S2 and S7 terminates S3 records 
    fprintf(pFile, "S%1X%02X%0*X",10-data_rectype, addr_width/2 + 1, addr_width, 0);
    fprintf(pFile, "%02X\n", srec_csum(buf, 0, addr_width/2 + 1, 0));
	

}
