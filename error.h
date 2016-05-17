

// Fixes warning
#undef ERROR

#define ERROR(s) do { fprintf(stderr, "%s\n", (s)); exit(-1); } while(0)
#define ERROR2(...) do { fprintf(stderr, __VA_ARGS__); exit(-1); } while(0)
#define PERROR(s) do { perror((s)); exit(-1); } while(0)
#define USAGE_ERROR(s) do { fprintf(stderr, "%s\n", (s)); print_help_and_exit(argv[0]); } while(0)
