#include "error.h"

#define TRY(times, statement, where) do { 		\
	int c = (times);			\
	while(c > 0) {				\
		usleep(10000);			\
		if((statement)) break;		\
		c--;				\
	}					\
	if(!c) {				\
		ERROR("Tries exceeded doing " where);	\
	}					\
} while(0)
