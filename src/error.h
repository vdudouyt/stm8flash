#ifndef __ERROR_H
#define __ERROR_H

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern int g_dbg_level;
#define DBG(...) do { if (g_dbg_level > 1) { _out(__FUNCTION__, __LINE__, "  DBG", __VA_ARGS__); } } while(0)
#define ERR(...) _out(__FUNCTION__, __LINE__, "ERROR", __VA_ARGS__)
#define WARN(...) _out(__FUNCTION__, __LINE__, " WARN", __VA_ARGS__)
#define INFO(...) _out(__FUNCTION__, __LINE__, " INFO", __VA_ARGS__)
static inline void _out(const char *file, int line, const char *msg, const char *fmt, ...) {
	int len = strlen(file);
	fprintf(stderr, "%s", file);
	for (;len < 20; len++) {
		fprintf(stderr, " ");
	}
	fprintf(stderr, " [% 5d]: %s: ", line, msg);
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	fprintf(stderr, "\n");
}

#endif
