/* stlink/v2 stm8 memory programming utility
   (c) Valentin Dudouyt, 2012 - 2014 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "srec.h"
#include "error.h"
#include "byte_utils.h"

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
char line[516];

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


static bool is_data_type(unsigned int chunk_type)
{
  return (chunk_type == 0x01 || chunk_type == 0x02 || chunk_type == 0x03);
}



int srec_read(FILE *pFile, unsigned char *buf, unsigned int start, unsigned int end) {
  fseek(pFile, 0, SEEK_SET);
  unsigned int chunk_len, chunk_addr, chunk_type, i, byte, line_no = 0, greatest_addr = 0;
  unsigned int number_of_records = 0;

  bool data_record = false;
  bool found_S5_rec = false;
  unsigned int expected_data_records = 0;
  unsigned int data_len = 0;
  unsigned char temp = ' ';


  while(fgets(line, sizeof(line), pFile)) {
    data_record = true; //Assume that the read data is a data record. Set to false if not.
    line_no++;

    // Reading chunk header
    if(sscanf(line, "S%01x%02x%08x", &chunk_type, &chunk_len, &chunk_addr) != 3) {
      sscanf(line, "%c",&temp);
      if(temp != 'S')
      {
        continue;
      } 
      free(buf);
      ERROR2("Error while parsing SREC at line %d\n", line_no);
    }
    
    if(chunk_type == 0x00 || chunk_type == 0x04) //Header type record or reserved. Skip!
      {
	continue;
      }
    else if(chunk_type == 0x05)
      { //This will contain the total expected number of data records. Save for error checking later
	found_S5_rec = true;
	if (chunk_len != 3) { // Length field must contain a 3 to be a valid S5 record.
	  ERROR2("Error while parsing S5 Record at line %d\n", line_no);
	}
	expected_data_records = chunk_addr; //The expected total number of data records is saved in S503<addr> field.
      }
    else if(is_data_type(chunk_type))
      {
	chunk_addr = chunk_addr >> (8*(3-chunk_type));
	data_len = chunk_len - chunk_type - 2; // See https://en.wikipedia.org/wiki/SREC_(file_format)#Record_types
      }
    else
      {
	continue;
      }

    // Reading chunk data
    for(i = 6 + 2*chunk_type; i < 2*data_len + 8; i +=2) {
      if(sscanf(&(line[i]), "%02x", &byte) != 1) {
	free(buf);
	ERROR2("Error while parsing SREC at line %d byte %d\n", line_no, i);
      }

      if(!is_data_type(chunk_type)) {
	// The only data records have to be processed
	data_record = false;
	continue;
      }

      if((i - 8) / 2 >= chunk_len) {
	// Respect chunk_len and do not capture checksum as data
	break;
      }
      if(chunk_addr < start) {
	free(buf);
	ERROR2("Address %08x is out of range at line %d\n", chunk_addr, line_no);
      }
      if(chunk_addr + data_len > end) {
	free(buf);
	ERROR2("Address %08x + %d is out of range at line %d\n", chunk_addr, data_len, line_no);
      }
      if(chunk_addr + data_len > greatest_addr) {
	greatest_addr = chunk_addr + data_len;
      }
      buf[chunk_addr - start + (i - 8) / 2] = byte;
    }
    if(data_record) { //We found a data record. Remember this.
      number_of_records++;
    }
  }

  //Check to see if the number of data records expected is the same as we actually read.
  if(found_S5_rec && number_of_records != expected_data_records) {
    ERROR2("Error while comparing S5 record (number of data records: %d) with the number actually read: %d\n", expected_data_records, number_of_records);
  }

  return(greatest_addr - start);

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
