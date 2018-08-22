/* stlink/v2 stm8 memory programming utility
   (c) Valentin Dudouyt, 2012 - 2014 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include "pgm.h"
#include "espstlink.h"
#include "stlink.h"
#include "stlinkv2.h"
#include "stm8.h"
#include "ihex.h"
#include "srec.h"

typedef enum {
    INTEL_HEX = 0,
    MOTOROLA_S_RECORD,
    RAW_BINARY
} fileformat_t;

#ifdef __APPLE__
extern char *optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;
#endif

#define VERSION_RELASE_DATE "20170616"
#define VERSION "1.1"
#define VERSION_NOTES ""

programmer_t pgms[] = {
	{ 	"stlink",
		0x0483, // USB vid
		0x3744, // USB pid
		stlink_open,
		stlink_close,
		stlink_swim_srst,
		stlink_swim_read_range,
		stlink_swim_write_range,
	},
	{
		"stlinkv2",
		0x0483,
		0x3748,
		stlink2_open,
		stlink_close,
		stlink2_srst,
		stlink2_swim_read_range,
		stlink2_swim_write_range,
	},
    {
		"espstlink",
		0,
		0,
		espstlink_pgm_open,
		espstlink_pgm_close,
		espstlink_srst,
		espstlink_swim_read_range,
		espstlink_swim_write_range,
	},
	{ NULL },
};

void print_help_and_exit(const char *name, bool err) {
	FILE *stream = err ? stderr : stdout;
	fprintf(stream, "Usage: %s [-c programmer] [-p partno] [-s memtype] [-b bytes] [-r|-w|-v] <filename>\n", name);
	fprintf(stream, "Options:\n");
	fprintf(stream, "\t-?             Display this help\n");
	fprintf(stream, "\t-c programmer  Specify programmer used (stlink, stlinkv2 or espstlink)\n");
	fprintf(stream, "\t-d port        Specify the serial device for espstlink (default: /dev/ttyUSB0)\n");
	fprintf(stream, "\t-p partno      Specify STM8 device\n");
	fprintf(stream, "\t-l             List supported STM8 devices\n");
	fprintf(stream, "\t-s memtype     Specify memory type (flash, eeprom, ram, opt or explicit address)\n");
	fprintf(stream, "\t-b bytes       Specify number of bytes\n");
	fprintf(stream, "\t-r <filename>  Read data from device to file\n");
	fprintf(stream, "\t-w <filename>  Write data from file to device\n");
	fprintf(stream, "\t-v <filename>  Verify data in device against file\n");
	fprintf(stream, "\t-V             Print Date(YearMonthDay-Version) and Version format is IE: 20171204-1.0\n");
	fprintf(stream, "\t-u             Unlock. Reset option bytes to factory default to remove write protection.\n");
	exit(-err);
}

void print_version_and_exit( bool err) {
	FILE *stream = err ? stderr : stdout;
	fprintf(stream, "%s-%s\n%s",VERSION_RELASE_DATE, VERSION, VERSION_NOTES );
	exit(-err);
}


void spawn_error(const char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(-1);
}

void dump_pgms(programmer_t *pgms) {
	// Dump programmers list in stderr
	int i;
	for(i = 0; pgms[i].name; i++)
		fprintf(stderr, "%s\n", pgms[i].name);
}

bool is_ext(const char *filename, const char *ext) {
	char *ext_begin = strrchr(filename, '.');
	return(ext_begin && strcmp(ext_begin, ext) == 0);
}

bool usb_init(programmer_t *pgm) {
	if (!pgm->usb_vid && !pgm->usb_pid) return(true);

	libusb_device **devs;
	libusb_context *ctx = NULL;

	int r;
	ssize_t cnt;
	r = libusb_init(&ctx);
	if(r < 0) return(false);

#ifdef STM8FLASH_LIBUSB_QUIET
	libusb_set_debug(ctx, 0);
#else
	libusb_set_debug(ctx, 3);
#endif
	cnt = libusb_get_device_list(ctx, &devs);
	if(cnt < 0) return(false);

	pgm->dev_handle = libusb_open_device_with_vid_pid(ctx, pgm->usb_vid, pgm->usb_pid);
	pgm->ctx = ctx;
	if (!pgm->dev_handle) spawn_error("Could not open USB device.");
	// assert(pgm->dev_handle);

	libusb_free_device_list(devs, 1); //free the list, unref the devices in it

	if(libusb_kernel_driver_active(pgm->dev_handle, 0) == 1) { //find out if kernel driver is attached
		int r = libusb_detach_kernel_driver(pgm->dev_handle, 0);
		assert(r == 0);
	}

#if defined(__APPLE__) || defined(WIN32)
	r = libusb_claim_interface(pgm->dev_handle, 0);
	assert(r == 0);
#endif

	return(true);
}

const stm8_device_t *get_part(const char *name)
{
	for(unsigned int i = 0; stm8_devices[i].name; i++)
	{
		const char *e = stm8_devices[i].name;
		const char *s = name;
		for(e = stm8_devices[i].name, s = name; *s && (*e == *s || *e == '?'); e++, s++);
		if(!*e)
			return(&stm8_devices[i]);
	}
	return(0);
}

int main(int argc, char **argv) {
	unsigned int start;
	int bytes_count = 0;
	char filename[256];
	memset(filename, 0, sizeof(filename));
	// Parsing command line
	char c;
	action_t action = NONE;
	fileformat_t fileformat = RAW_BINARY;
	bool start_addr_specified = false,
		pgm_specified = false,
		part_specified = false,
        bytes_count_specified = false;
	memtype_t memtype = FLASH;
	const char * port = NULL;
	int i;
	programmer_t *pgm = NULL;
	const stm8_device_t *part = NULL;
	while((c = getopt (argc, argv, "r:w:v:nc:p:d:s:b:luV")) != (char)-1) {
		switch(c) {
			case 'c':
				pgm_specified = true;
				for(i = 0; pgms[i].name; i++) {
					if(!strcmp(optarg, pgms[i].name))
						pgm = &pgms[i];
				}
				break;
			case 'p':
				part_specified = true;
				part = get_part(optarg);
				break;
			case 'd':
				port = strdup(optarg);
				break;
			case 'l':
				for(i = 0; stm8_devices[i].name; i++)
					printf("%s ", stm8_devices[i].name);
				printf("\n");
				exit(0);
			case 'r':
				action = READ;
				strcpy(filename, optarg);
				break;
			case 'w':
				action = WRITE;
				strcpy(filename, optarg);
				break;
			case 'v':
				action = VERIFY;
				strcpy(filename, optarg);
				break;
                        case 'u':
				action = UNLOCK;
				start  = 0x4800;
				memtype = OPT;
				strcpy(filename, "Workaround");
				break;
			case 's':
				// Start addr is depending on MCU type
				if(strcasecmp(optarg, "flash") == 0)
					memtype = FLASH;
				else if(strcasecmp(optarg, "eeprom") == 0)
					memtype = EEPROM;
				else if(strcasecmp(optarg, "ram") == 0)
					memtype = RAM;
				else if(strcasecmp(optarg, "opt") == 0)
					memtype = OPT;
				else {
					// Start addr is specified explicitely
					memtype = UNKNOWN;
					if(sscanf(optarg, "%x", (unsigned*)&start) != strlen(optarg))
						spawn_error("Invalid memory type or location specified");
					start_addr_specified = true;
				}
				break;
			case 'b':
				bytes_count = atoi(optarg);
				bytes_count_specified = true;
				break;
			case 'V':
                                print_version_and_exit( (bool)0);
				break;
			case '?':
                                print_help_and_exit(argv[0], false);
			default:
				print_help_and_exit(argv[0], true);
		}
	}
	if(argc <= 1)
		print_help_and_exit(argv[0], true);
	if(pgm_specified && !pgm) {
		fprintf(stderr, "No valid programmer specified. Possible values are:\n");
		dump_pgms( (programmer_t *) &pgms);
		exit(-1);
	}
	if(!pgm)
		spawn_error("No programmer has been specified");
	pgm->port = port;
	if(part_specified && !part) {
		fprintf(stderr, "No valid part specified. Use -l to see the list of supported devices.\n");
		exit(-1);
	}
	if(!part)
		spawn_error("No part has been specified");

	// Try define memory type by address
	if(memtype == UNKNOWN) {
        if((start >= 0x4800) && (start < 0x4880)) {
            memtype = OPT;
        }
        if((start >= part->ram_start) && (start < part->ram_start + part->ram_size)) {
            memtype = RAM;
        }
        else if((start >= part->flash_start) && (start < part->flash_start + part->flash_size)) {
            memtype = FLASH;
        }
        else if((start >= part->eeprom_start) && (start < part->eeprom_start + part->eeprom_size)) {
            memtype = EEPROM;
        }
    }

	if(memtype != UNKNOWN) {
		// Selecting start addr depending on
		// specified part and memtype
		switch(memtype) {
			case RAM:
                if(!start_addr_specified) {
                    start = part->ram_start;
                }
                if(!bytes_count_specified || bytes_count > part->ram_size) {
                    bytes_count = part->ram_size;
                }
                fprintf(stderr, "Determine RAM area\r\n");
				break;
			case EEPROM:
                if(!start_addr_specified) {
                    start = part->eeprom_start;
                }
                if(!bytes_count_specified || bytes_count > part->eeprom_size) {
                    bytes_count = part->eeprom_size;
                }
                fprintf(stderr, "Determine EEPROM area\r\n");
				break;
			case FLASH:
                if(!start_addr_specified) {
                    start = part->flash_start;
                }
                if(!bytes_count_specified || bytes_count > part->flash_size) {
                    bytes_count = part->flash_size;
                }
                fprintf(stderr, "Determine FLASH area\r\n");
				break;
			case OPT:
                if(!start_addr_specified) {
                    start = 0x4800;
                }
                size_t opt_size = (part->flash_size <= 8*1024 ? 0x40 : 0x80);
                if(!bytes_count_specified || bytes_count > opt_size) {
                    bytes_count = opt_size;
                }
                fprintf(stderr, "Determine OPT area\r\n");
                break;
		}
		start_addr_specified = true;
	}
	if(!action)
		spawn_error("No action has been specified");
	if(!start_addr_specified)
		spawn_error("No memtype or start_addr has been specified");
	if (!strlen(filename))
		spawn_error("No filename has been specified");
	if(!action || !start_addr_specified || !strlen(filename))
		print_help_and_exit(argv[0], true);
	if(!usb_init(pgm))
		spawn_error("Couldn't initialize stlink");
	if(!pgm->open(pgm))
		spawn_error("Error communicating with MCU. Please check your SWIM connection.");

	if(is_ext(filename, ".ihx") || is_ext(filename, ".hex") || is_ext(filename, ".i86"))
		fileformat = INTEL_HEX;
	else if(is_ext(filename, ".s19") || is_ext(filename, ".s8") || is_ext(filename, ".srec"))
		fileformat = MOTOROLA_S_RECORD;
	fprintf(stderr, "Due to its file extension (or lack thereof), \"%s\" is considered as %s format!\n", filename, fileformat == INTEL_HEX ? "INTEL HEX" : (fileformat == MOTOROLA_S_RECORD ? "MOTOROLA S-RECORD" : "RAW BINARY"));

	FILE *f;
	if(action == READ) {
		fprintf(stderr, "Reading %d bytes at 0x%x... ", bytes_count, start);
		fflush(stderr);
		int bytes_count_align = ((bytes_count-1)/256+1)*256; // Reading should be done in blocks of 256 bytes
		unsigned char *buf = malloc(bytes_count_align);
		if(!buf) spawn_error("malloc failed");
		int recv = pgm->read_range(pgm, part, buf, start, bytes_count_align);
		if(recv < bytes_count_align) {
			fprintf(stderr, "\r\nRequested %d bytes but received only %d.\r\n", bytes_count_align, recv);
			spawn_error("Failed to read MCU");
		}
		if(!(f = fopen(filename, "w")))
			spawn_error("Failed to open file");
		switch(fileformat)
		{
		case INTEL_HEX:
			ihex_write(f, buf, start, start+bytes_count);
			break;
		case MOTOROLA_S_RECORD:
			srec_write(f, buf, start, start+bytes_count);
			break;
		default:
			fwrite(buf, 1, bytes_count, f);
		}
		fclose(f);
		fprintf(stderr, "OK\n");
		fprintf(stderr, "Bytes received: %d\n", bytes_count);
	} else if (action == VERIFY) {
		fprintf(stderr, "Verifing %d bytes at 0x%x... ", bytes_count, start);
		fflush(stderr);

		int bytes_count_align = ((bytes_count-1)/256+1)*256; // Reading should be done in blocks of 256 bytes
		unsigned char *buf = malloc(bytes_count_align);
		if(!buf) spawn_error("malloc failed");
		int recv = pgm->read_range(pgm, part, buf, start, bytes_count_align);
		if(recv < bytes_count_align) {
			fprintf(stderr, "\r\nRequested %d bytes but received only %d.\r\n", bytes_count_align, recv);
			spawn_error("Failed to read MCU");
		}

		if(!(f = fopen(filename, "r")))
			spawn_error("Failed to open file");
		unsigned char *buf2 = malloc(bytes_count);
		if(!buf2) spawn_error("malloc failed");
		int bytes_to_verify;
		/* reading bytes to RAM */
		switch(fileformat)
		{
		case INTEL_HEX:
			bytes_to_verify = ihex_read(f, buf2, start, start + bytes_count);
			break;
		case MOTOROLA_S_RECORD:
			bytes_to_verify = srec_read(f, buf2, start, start + bytes_count);
			break;
		default:
			fseek(f, 0L, SEEK_END);
			bytes_to_verify = ftell(f);
			if(bytes_count_specified)
				bytes_to_verify = bytes_count;
			else if(bytes_count < bytes_to_verify)
				bytes_to_verify = bytes_count;
			fseek(f, 0, SEEK_SET);
			fread(buf2, 1, bytes_to_verify, f);
		}
		fclose(f);

		if(memcmp(buf, buf2, bytes_to_verify) == 0) {
			fprintf(stderr, "OK\n");
			fprintf(stderr, "Bytes verified: %d\n", bytes_to_verify);
		} else {
			fprintf(stderr, "FAILED\n");
			exit(-1);
		}


	} else if (action == WRITE) {
		if(!(f = fopen(filename, "r")))
			spawn_error("Failed to open file");
		int bytes_count_align = ((bytes_count-1)/part->flash_block_size+1)*part->flash_block_size;
		unsigned char *buf = malloc(bytes_count_align);
		if(!buf) spawn_error("malloc failed");
		memset(buf, 0, bytes_count_align); // Clean aligned buffer
		int bytes_to_write;

		/* reading bytes to RAM */
		switch(fileformat)
		{
		case INTEL_HEX:
			bytes_to_write = ihex_read(f, buf, start, start + bytes_count);
			break;
		case MOTOROLA_S_RECORD:
			bytes_to_write = srec_read(f, buf, start, start + bytes_count);
			break;
		default:
			fseek(f, 0L, SEEK_END);
			bytes_to_write = ftell(f);
			if(bytes_count_specified)
				bytes_to_write = bytes_count;
			else if(bytes_count < bytes_to_write)
				bytes_to_write = bytes_count;
			fseek(f, 0, SEEK_SET);
			fread(buf, 1, bytes_to_write, f);
		}
		fprintf(stderr, "%d bytes at 0x%x... ", bytes_to_write, start);

		/* flashing MCU */
		int sent = pgm->write_range(pgm, part, buf, start, bytes_to_write, memtype);
		if(pgm->reset) {
			// Restarting core (if applicable)
			pgm->reset(pgm);
		}
		fprintf(stderr, "OK\n");
		fprintf(stderr, "Bytes written: %d\n", sent);
		fclose(f);
	} else if (action == UNLOCK) {
		int bytes_to_write=part->option_bytes_size;

		if (part->read_out_protection_mode == ROP_UNKNOWN) spawn_error("No unlocking mode defined for this device. You may need to edit the file stm8.c");
		if (part->read_out_protection_mode != ROP_STM8S) spawn_error("Unimplemented unlocking mode");

		unsigned char *buf=malloc(bytes_to_write);
		if(!buf) spawn_error("malloc failed");

		if (part->read_out_protection_mode == ROP_STM8S) {
			for (int i=0; i<bytes_to_write;i++) {
				buf[i]=0;
				if ((i>0)&&((i&1)==0)) buf[i]=0xff;
			}
		}

		/* flashing MCU */
		int sent = pgm->write_range(pgm, part, buf, start, bytes_to_write, memtype);
		if(pgm->reset) {
			// Restarting core (if applicable)
			pgm->reset(pgm);
		}
		fprintf(stderr, "Unlocked device. Option bytes reset to default state.\n");
		fprintf(stderr, "Bytes written: %d\n", sent);
	}
	return(0);
}
