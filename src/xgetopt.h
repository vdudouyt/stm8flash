/* xgetopt: like getopt(3), but compact, freestanding, and no globals
 * Behaves as though opterr is zero and optstring begins with ':'.
 * This is free and unencumbered software released into the public domain.
 */
#ifndef XGETOPT_H
#define XGETOPT_H

#define XGETOPT_INIT                                                                                                                                                               \
	{ 0, 0, 0, 0 } /* initialize to all zeroes */
struct xgetopt {
	char *optarg;
	int optind, optopt, optpos;
};

static int xgetopt(struct xgetopt *x, int argc, char **argv, const char *optstring) {
	char *arg = argv[!x->optind ? (x->optind += !!argc) : x->optind];
	if (arg && arg[0] == '-' && arg[1] == '-' && !arg[2]) {
		x->optind++;
		return -1;
	} else if (!arg || arg[0] != '-' || ((arg[1] < '0' || arg[1] > '9') && (arg[1] < 'A' || arg[1] > 'Z') && (arg[1] < 'a' || arg[1] > 'z'))) {
		return -1;
	} else {
		while (*optstring && arg[x->optpos + 1] != *optstring) {
			optstring++;
		}
		x->optopt = arg[x->optpos + 1];
		if (!*optstring) {
			return '?';
		} else if (optstring[1] == ':') {
			if (arg[x->optpos + 2]) {
				x->optarg = arg + x->optpos + 2;
				x->optind++;
				x->optpos = 0;
				return x->optopt;
			} else if (argv[x->optind + 1]) {
				x->optarg = argv[x->optind + 1];
				x->optind += 2;
				x->optpos = 0;
				return x->optopt;
			} else {
				return ':';
			}
		} else {
			if (!arg[++x->optpos + 1]) {
				x->optind++;
				x->optpos = 0;
			}
			return x->optopt;
		}
	}
}

#endif
