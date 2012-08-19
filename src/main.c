/* stlink/v2 stm8 memory programming utility
   (c) Valentin Dudouyt, 2012 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "stlink.h"

void print_help_and_exit(const char *name) {
	fprintf(stderr, "Usage: %s [-r|-w] [-s start] [-f filename] [-b bytes_count]\n", name);
	exit(-1);
}

void spawn_error(const char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(-1);
}

int main(int argc, char **argv) {
	int start;
	char filename[256];
	memset(filename, 0, sizeof(filename));
	// Parsing command line
	char c;
	enum {
		NONE = 0,
		READ,
		WRITE
	} action = NONE;
	bool start_addr_specified = false;
	int bytes_count = 0;
	while((c = getopt (argc, argv, "rws:f:b:")) != -1) {
		switch(c) {
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
				sscanf(optarg, "%x", &start);
				break;
			case 'b':
				bytes_count = atoi(optarg);
				break;
			default:
				print_help_and_exit(argv[0]);
		}
	}
	if(!action || !start_addr_specified || !strlen(filename))
		print_help_and_exit(argv[0]);
//	printf("Direction: %d start: %x filename: %s\n", action, start, filename);
//	return(0);

	stlink_context_t *context = stlink_init(0);
	if(!context)
		spawn_error("Couldn't initialize stlink");
	int r = stlink_swim_start(context);
	if(r != STLK_OK)
		spawn_error("Error communicating with MCU. Please check your SWIM connection.");
	FILE *f;
	if(action == READ) {
		fprintf(stderr, "Reading at 0x%x... ", start);
		fflush(stderr);
		char *buf = malloc(bytes_count);
		if(!buf) spawn_error("malloc failed");
		int recv = stlink_swim_read_range(context, buf, start, bytes_count);
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
		int sent = stlink_swim_write_range(context, buf, start, bytes_count);
		fprintf(stderr, "OK\n");
		fprintf(stderr, "Bytes written: %d\n", sent);
		fclose(f);
	}
	return(0);
}
