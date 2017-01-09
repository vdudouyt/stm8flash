/* stlink/v2 stm8 memory programming utility
   (c) Valentin Dudouyt, 2012 - 2014 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "srec.h"
#include "error.h"
#include "byte_utils.h"

char line[256];

static unsigned char checksum(unsigned char *buf, unsigned int length, int chunk_len, int chunk_addr, int chunk_type) {
  int sum = chunk_len + (LO(chunk_addr)) + (HI(chunk_addr)) + chunk_type;
  unsigned int i;
  for(i = 0; i < length; i++) {
    sum += buf[i];
  }

  int complement = (~sum + 1);
  return complement & 0xff;
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

  while(fgets(line, sizeof(line), pFile)) {
    data_record = true; //Assume that the read data is a data record. Set to false if not.
    line_no++;

    // Reading chunk header
    if(sscanf(line, "S%01x%02x%04x", &chunk_type, &chunk_len, &chunk_addr) != 3) {
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
	ERROR2("Address %04x is out of range at line %d\n", chunk_addr, line_no);
      }
      if(chunk_addr + chunk_len > end) {
	free(buf);
	ERROR2("Address %04x + %d is out of range at line %d\n", chunk_addr, chunk_len, line_no);
      }
      if(chunk_addr + chunk_len > greatest_addr) {
	greatest_addr = chunk_addr + chunk_len;
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
  ERROR("srec_write is not implemented!");
  /* Ducplicate most of the code from ihex.c ihex_write. */
}
