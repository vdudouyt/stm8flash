/* stlink/v2 stm8 memory programming utility
   (c) Valentin Dudouyt, 2012 - 2013 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include "pgm.h"
#include "stlink.h"

programmer_t pgms[] = {
	{ 	"stlink",
		0x0483,
		0x3744,
		stlink_open,
		stlink_close,
	},
	{ 
		"stlinkv2", 
		0x0483,
		0x3748,
		stlink2_open,
		stlink_close,
		stlink2_swim_read_range,
		stlink2_swim_write_range,
	},
	{ NULL },
};

void print_help_and_exit(const char *name) {
	fprintf(stderr, "Usage: %s [-c programmer] [-r|-w] [-s memtype] [-f filename] [-b bytes_count]\n", name);
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
		pgm_specified = false;
	int i;
	programmer_t *pgm = NULL;
	while((c = getopt (argc, argv, "rwc:s:f:b:")) != -1) {
		switch(c) {
			case 'c':
				pgm_specified = true;
				for(i = 0; pgms[i].name; i++) {
					if(!strcmp(optarg, pgms[i].name))
						pgm = &pgms[i];
				}
				break;
			case 'r':
				action = READ;
				break;
			case 'w':
				action = WRITE;
				break;
			case 'f':
				strcpy(filename, optarg);
				break;
			case 's':
				start_addr_specified = true;
				assert(sscanf(optarg, "%x", &start));
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
	if(!action)
		spawn_error("No action has been specified");
	if(!start_addr_specified)
		spawn_error("No start_addr has been specified");
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
		fprintf(stderr, "OK\n");
		fprintf(stderr, "Bytes written: %d\n", sent);
		fclose(f);
	}
	return(0);
}
