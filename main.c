/* stlink/v2 stm8 memory programming utility
   (c) Valentin Dudouyt, 2012 - 2014 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include "pgm.h"
#include "stlink.h"
#include "stm8.h"

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
		NULL,
		stlink2_swim_read_range,
		stlink2_swim_write_range,
	},
	{ NULL },
};

stm8_mcu_spec_t parts[] = {
	{ "stm8s003", 0x0000, 1*1024, 0x4000, 128, 0x8000, 8*1024 },
	{ "stm8s103", 0x0000, 1*1024, 0x4000, 640, 0x8000, 8*1024 },
	{ "stm8s105", 0x0000, 2*1024, 0x4000, 1024, 0x8000, 16*1024 },
	{ "stm8l150", 0x0000, 2*1024, 0x1000, 1024, 0x8000, 32*1024 },
	{ NULL },
};

void print_help_and_exit(const char *name) {
	fprintf(stderr, "Usage: %s [-c programmer] [-p partno] [-r|-w] [-s memtype] [-f filename] [-b bytes_count]\n", name);
	exit(-1);
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

void dump_parts(stm8_mcu_spec_t *parts) {
	// Dump parts list in stderr
	int i;
	for(i = 0; parts[i].name; i++)
		fprintf(stderr, "%s\n", parts[i].name);
}

bool usb_init(programmer_t *pgm, unsigned int vid, unsigned int pid) {
	libusb_device **devs;
	libusb_context *ctx = NULL;

	int r;
	ssize_t cnt;
	r = libusb_init(&ctx);
	if(r < 0) return(false);

	libusb_set_debug(ctx, 3);
	cnt = libusb_get_device_list(ctx, &devs);
	if(cnt < 0) return(false);

	pgm->dev_handle = libusb_open_device_with_vid_pid(ctx, vid, pid);
	pgm->ctx = ctx;
	assert(pgm->dev_handle);

	libusb_free_device_list(devs, 1); //free the list, unref the devices in it

	if(libusb_kernel_driver_active(pgm->dev_handle, 0) == 1) { //find out if kernel driver is attached
		int r = libusb_detach_kernel_driver(pgm->dev_handle, 0);
		assert(r == 0);
	}
	return(true);
}

int main(int argc, char **argv) {
	int start, bytes_count = 0;
	char filename[256];
	memset(filename, 0, sizeof(filename));
	// Parsing command line
	char c;
	enum {
		NONE = 0,
		READ,
		WRITE
	} action = NONE;
	bool start_addr_specified = false,
		pgm_specified = false,
		part_specified = false;
	enum {
		UNKNOWN,
		RAM,
		EEPROM,
		FLASH,
	} memtype = FLASH;
	int i;
	programmer_t *pgm = NULL;
	stm8_mcu_spec_t *part = NULL;
	while((c = getopt (argc, argv, "r:w:nc:p:s:b:")) != -1) {
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
				for(i = 0; parts[i].name; i++) {
					if(!strcmp(optarg, parts[i].name))
						part = &parts[i];
				}
				break;
			case 'r':
				action = READ;
				strcpy(filename, optarg);
				break;
			case 'w':
				action = WRITE;
				strcpy(filename, optarg);
				break;
			case 's':
				start_addr_specified = true;
				if(!strcmp(optarg, "flash")) {
					// Start addr is depending on MCU type
					memtype = FLASH;
				} else {
					// Start addr is specified explicitely
					memtype = UNKNOWN;
					int success = sscanf(optarg, "%x", &start);
					assert(success);
				}
				break;
			case 'b':
				bytes_count = atoi(optarg);
				break;
			default:
				print_help_and_exit(argv[0]);
		}
	}
	if(argc <= 1)
		print_help_and_exit(argv[0]);
	if(pgm_specified && !pgm) {
		fprintf(stderr, "No valid programmer specified. Possible values are:\n");
		dump_pgms( (programmer_t *) &pgms);
		exit(-1);
	}
	if(!pgm)
		spawn_error("No programmer has been specified");
	if(part_specified && !part) {
		fprintf(stderr, "No valid part specified. Possible values are:\n");
		dump_parts( (stm8_mcu_spec_t *) &parts);
		exit(-1);
	}
	if(!part)
		spawn_error("No part has been specified");

	if(memtype != UNKNOWN) {
		// Selecting start addr depending on 
		// specified part and memtype
		start_addr_specified = true;
		switch(memtype) {
			case RAM:
				start = part->ram_start;
				bytes_count = part->ram_size;
				break;
			case EEPROM:
				start = part->eeprom_start;
				bytes_count = part->eeprom_size;
				break;
			case FLASH:
				start = part->flash_start;
				bytes_count = part->flash_size;
				break;
		}
	}
	if(!action)
		spawn_error("No action has been specified");
	if(!start_addr_specified)
		spawn_error("No memtype or start_addr has been specified");
	if(!strlen(filename))
		spawn_error("No filename has been specified");
	if(!action || !start_addr_specified || !strlen(filename))
		print_help_and_exit(argv[0]);
	if(!usb_init(pgm, pgm->usb_vid, pgm->usb_pid))
		spawn_error("Couldn't initialize stlink");
	if(!pgm->open(pgm))
		spawn_error("Error communicating with MCU. Please check your SWIM connection.");
	FILE *f;
	if(action == READ) {
		fprintf(stderr, "Reading at 0x%x... ", start);
		fflush(stderr);
		char *buf = malloc(bytes_count);
		if(!buf) spawn_error("malloc failed");
		int recv = pgm->read_range(pgm, buf, start, bytes_count);
		f = fopen(filename, "w");
		fwrite(buf, 1, recv, f);
		fclose(f);
		fprintf(stderr, "OK\n");
		fprintf(stderr, "Bytes received: %d\n", recv);
	} else if (action == WRITE) {
		fprintf(stderr, "Writing at 0x%x... ", start);
		f = fopen(filename, "r");
		fseek(f, 0L, SEEK_END);
		/* preparing buffer */
		bytes_count = ftell(f);
		char *buf = malloc(bytes_count);
		if(!buf) spawn_error("malloc failed");
		fseek(f, 0, SEEK_SET);
		fread(buf, 1, bytes_count, f);
		/* flashing MCU */
		int sent = pgm->write_range(pgm, buf, start, bytes_count);
		if(pgm->reset) {
			// Restarting core (if applicable)
			pgm->reset(pgm);
		}
		fprintf(stderr, "OK\n");
		fprintf(stderr, "Bytes written: %d\n", sent);
		fclose(f);
	}
	return(0);
}
