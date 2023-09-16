#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "adapter.h"
#include "binary.h"
#include "error.h"
#include "ihex.h"
#include "region.h"
#include "srec.h"
#include "stm8.h"
#include "xgetopt.h"

#define QUOTE(str) #str
#define EXPAND_AND_QUOTE(str) QUOTE(str)

int g_dbg_level = 0;

#ifdef STM8FLASH_LIBUSB_QUIET
#define STM8FLASH_LIBUSB_DEBUG_LEVEL 0
#else
#define STM8FLASH_LIBUSB_DEBUG_LEVEL 3
#endif
#if defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01000106)
#define libusb_set_debug(ctx, usb_debug_level) libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, usb_debug_level);
#endif

static inline uint32_t minimum(uint32_t a, uint32_t b) { return a > b ? b : a; }

static memtype_t determine_memtype(const struct stm8_part *const part, const uint32_t requested_start) {
	memtype_t memtype = MEM_UNKNOWN;
	if ((requested_start >= part->opt_start) && (requested_start < part->opt_start + part->opt_size)) {
		memtype = MEM_OPT;
	} else if ((requested_start >= part->ram_start) && (requested_start < part->ram_start + part->ram_size)) {
		memtype = MEM_RAM;
	} else if ((requested_start >= part->flash_start) && (requested_start < part->flash_start + part->flash_size)) {
		memtype = MEM_FLASH;
	} else if ((requested_start >= part->eeprom_start) && (requested_start < part->eeprom_start + part->eeprom_size)) {
		memtype = MEM_EEPROM;
	}
	return memtype;
}

static int aligned_write(struct adapter *const pgm, const struct stm8_part *const part, const uint8_t *const data, const uint32_t start, const uint32_t end) {
	// writes are done in the chunks of part blocksize size to guarantee the correct memtype is selected
	const uint32_t BLOCK_SIZE = part->flash_block_size;
	assert(start <= end);
	for (uint32_t addr = start; addr < end;) {
		uint8_t block[BLOCK_SIZE];
		const uint32_t block_start = (addr / BLOCK_SIZE) * BLOCK_SIZE;
		const uint32_t block_end = block_start + BLOCK_SIZE;
		const uint32_t count = minimum(BLOCK_SIZE - (addr % BLOCK_SIZE), end - addr);

		if (stm8_read_block(pgm, part, block_start, block, BLOCK_SIZE)) {
			ERR("read failed");
			return -1;
		}

		if (memcmp(&block[addr % BLOCK_SIZE], &data[addr - start], count) == 0) {
			// do not write data if it is already correct
			INFO("SKIP");
		} else {
			int fast = 1;
			for (int i = 0; i < count; i++) {
				if (block[(addr % BLOCK_SIZE) + i] != 0) {
					fast = 0;
				}
			}

			memcpy(&block[addr % BLOCK_SIZE], &data[addr - start], count);

			DBG("WRITE [0x%04X:0x%04X]", block_start, block_start + BLOCK_SIZE);

			const memtype_t memtype = determine_memtype(part, block_start);
			if (memtype == MEM_UNKNOWN) {
				ERR("unable to determine memtype... no idea how to write this.");
				return -1;
			}

			if (stm8_write_block(pgm, part, block, BLOCK_SIZE, block_start, memtype, fast)) {
				ERR("Failed to write");
				return -1;
			}
		}

		addr = block_end;
	}

	return 0;
}

static int aligned_read(struct adapter *const pgm, const struct stm8_part *const part, uint8_t *const data, const uint32_t start, const uint32_t end) {
	// read is done in "large" blocks for speed
	const uint32_t BLOCK_SIZE = 512;
	assert(start <= end);
	for (uint32_t addr = start; addr < end;) {
		uint8_t block[BLOCK_SIZE];
		const uint32_t block_start = (addr / BLOCK_SIZE) * BLOCK_SIZE;
		const uint32_t block_end = block_start + BLOCK_SIZE;
		const uint32_t count = minimum(BLOCK_SIZE - (addr % BLOCK_SIZE), end - addr);

		DBG("READ [0x%04X:0x%04X] %d", block_start, block_start + BLOCK_SIZE, count);

		if (stm8_read_block(pgm, part, block_start, block, BLOCK_SIZE)) {
			ERR("Failed to read");
			return -1;
		}

		DBG("DATA OFX: %04X", addr - start);
		memcpy(&data[addr - start], &block[addr % BLOCK_SIZE], count);
		DBG("[%d]= 0x%02X", count-1, data[count-1]);

		addr = block_end;
	}

	return 0;
}

typedef enum { FORMAT_UNSPECIFIED, FORMAT_INTEL_HEX, FORMAT_BINARY, FORMAT_MOTOROLA_S_RECORD, FORMAT_AUTODETECT } fileformat_t;

static void serialno_to_hex(const char *serialno, const int length, char *serialno_hex) {
	const char lk[] = "0123456789ABCDEF";
	for (int i = 0; i < length; i++) {
		serialno_hex[i * 2 + 0] = lk[(serialno[i] >> 4) & 0x0f];
		serialno_hex[i * 2 + 1] = lk[(serialno[i] >> 0) & 0x0f];
	}
}

static int is_ext(const char *const filename, const char *const ext) {
	char *ext_begin = strrchr(filename, '.');
	return ext_begin && (strcmp(ext_begin, ext) == 0);
}

static fileformat_t deduce_file_format_from_extension(const char *const filename) {
	fileformat_t fileformat;
	if (is_ext(filename, ".ihex") || is_ext(filename, ".ihx") || is_ext(filename, ".hex") || is_ext(filename, ".i86")) {
		fileformat = FORMAT_INTEL_HEX;
	} else if (is_ext(filename, ".s19") || is_ext(filename, ".s8") || is_ext(filename, ".srec")) {
		fileformat = FORMAT_MOTOROLA_S_RECORD;
	} else if (is_ext(filename, ".bin")) {
		fileformat = FORMAT_BINARY;
	} else {
		fileformat = FORMAT_UNSPECIFIED;
	}
	return fileformat;
}

static int load_from_file(struct region **const r, const char *const file, fileformat_t *fmt) {
	if (*fmt == FORMAT_AUTODETECT) {
		*fmt = deduce_file_format_from_extension(file);
	}

	if (fmt == FORMAT_UNSPECIFIED) {
		ERR("cannot load; no file format given");
		return -1;
	}

	if (*fmt == FORMAT_MOTOROLA_S_RECORD) {
		FILE *const fp = fopen(file, "rb");
		if (!fp) {
			ERR("Failed to open file: %s", file);
			return -1;
		}
		if (srec_read(fp, r)) {
			ERR("unable to read input file");
			fclose(fp);
			return -1;
		}
		fclose(fp);
	} else if (*fmt == FORMAT_INTEL_HEX) {
		FILE *const fp = fopen(file, "rb");
		if (!fp) {
			ERR("Failed to open file: %s", file);
			return -1;
		}
		if (ihex_read(fp, r)) {
			ERR("unable to read input file");
			fclose(fp);
			return -1;
		}
		fclose(fp);
	} else if (*fmt == FORMAT_BINARY) {
		FILE *const fp = fopen(file, "rb");
		if (!fp) {
			ERR("Failed to open file: %s", file);
			return -1;
		}
		if (binary_read(fp, r)) {
			ERR("unable to read input file");
			fclose(fp);
			return -1;
		}
		fclose(fp);
	} else {
		ERR("bad input file format");
		return -1;
	}

	return 0;
}

static int save_file(struct region *const r, const char *const file, fileformat_t fmt) {
	if (fmt == FORMAT_AUTODETECT) {
		fmt = deduce_file_format_from_extension(file);
	}

	if (fmt == FORMAT_UNSPECIFIED) {
		ERR("cannot save; no file format given");
		return -1;
	}

	if (fmt == FORMAT_MOTOROLA_S_RECORD) {
		FILE *const fp = fopen(file, "wb");
		if (!fp) {
			ERR("fopen");
			return -1;
		}
		if (srec_write(fp, r)) {
			fclose(fp);
			return -1;
		}
		fclose(fp);
	} else if (fmt == FORMAT_INTEL_HEX) {
		FILE *const fp = fopen(file, "wb");
		if (!fp) {
			ERR("fopen");
			return -1;
		}
		if (ihex_write(fp, r)) {
			fclose(fp);
			return -1;
		}
		fclose(fp);
	} else if (fmt == FORMAT_BINARY) {
		FILE *const fp = fopen(file, "wb");
		if (!fp) {
			ERR("fopen");
			return -1;
		}
		if (binary_write(fp, r)) {
			fclose(fp);
			return -1;
		}
		fclose(fp);
	} else {
		ERR("bad output file format");
		return -1;
	}

	return 0;
}

static const struct adapter_info *gat_adapter_info(const uint16_t vid, const uint16_t pid) {
	for (const struct adapter_info *pgm = &adapter_info[0]; pgm->name != NULL; pgm++) {
		if ((pgm->usb_pid == pid) && (pgm->usb_vid == vid)) {
			return pgm;
		}
	}
	return NULL;
}

static struct adapter *adapter_open(const char *const pgm_serialno) {
	libusb_device **devs;
	libusb_context *ctx = NULL;
	int numOfProgrammers = 0;
	char serialno[32];
	char serialno_hex[65] = {0};

	if (libusb_init(&ctx)) {
		return NULL;
	}

	libusb_set_debug(ctx, STM8FLASH_LIBUSB_DEBUG_LEVEL);

	const ssize_t cnt = libusb_get_device_list(ctx, &devs);
	if (cnt < 0) {
		return NULL;
	}

	// count available programmers
	for (int i = 0; i < cnt; i++) {
		struct libusb_device_descriptor desc;
		libusb_get_device_descriptor(devs[i], &desc);
		if (gat_adapter_info(desc.idVendor, desc.idProduct)) {
			numOfProgrammers++;
		}
	}

	if (numOfProgrammers > 1 && !pgm_serialno) {
		ERR("ERROR: More than one programmer found but no serial number given.");
		return NULL;
	}

	struct adapter *driver = NULL;
	for (int i = 0; i < cnt; i++) {
		struct libusb_device_descriptor desc;
		if (libusb_get_device_descriptor(devs[i], &desc) > 0) {
			ERR("ERROR: unable to get usb device descriptor.");
			return NULL;
		}

		const struct adapter_info *const info = gat_adapter_info(desc.idVendor, desc.idProduct);
		if (info) {
			char vendor[32];
			char device[32];
			libusb_device_handle *tempHandle;

			if (libusb_open(devs[i], &tempHandle) < 0) {
				ERR("ERROR: unable to open usb device.");
				return NULL;
			}

			if (libusb_get_string_descriptor_ascii(tempHandle, desc.iManufacturer, (unsigned char *)vendor, sizeof(vendor)) < 0) {
				ERR("ERROR: unable to get usb string descriptor.");
				return NULL;
			}
			if (libusb_get_string_descriptor_ascii(tempHandle, desc.iProduct, (unsigned char *)device, sizeof(device)) < 0) {
				ERR("ERROR: unable to get usb string descriptor.");
				return NULL;
			}
			const int serialno_length = libusb_get_string_descriptor_ascii(tempHandle, desc.iSerialNumber, (unsigned char *)serialno, sizeof(serialno));
			if (serialno_length < 0) {
				ERR("ERROR: unable to get usb string descriptor.");
				return NULL;
			}
			serialno_to_hex(serialno, serialno_length, serialno_hex);

			// print programmer data if no serial number specified
			if ((!pgm_serialno) || (0 == strcmp(serialno_hex, pgm_serialno))) {
				driver = info->open(ctx, tempHandle);
				if (driver) {
					break;
				}

				ERR("ERROR: unable to initialize driver.");
				return NULL;
			}
			libusb_close(tempHandle);
		}
	}

	libusb_free_device_list(devs, 1); // free the list, unref the devices in it

	if (!driver) {
		ERR("ERROR: No programmer found.");
		return NULL;
	}

	return driver;
}

static const struct stm8_part *get_part(const char *name) {
	const struct stm8_part *part = &stm8_devices[0];
	for (; part->name; part++) {
		const char *e = part->name;
		const char *s = name;
		while (*s && (*e == *s || toupper(*e) == *s || *e == '?')) {
			e++;
			s++;
		}
		if ((*e == *s) && (*s == 0)) {
			return part;
		}
	}
	return (0);
}

static int check_regions(struct region *dst, struct region *src, const int force) {
	// make sure src is contained in of dst
	for (struct region *s = src; s; s = s->next) {
		uint32_t start = s->start;
		while (start < s->end) {
			int found = 0;
			for (struct region *d = dst; d; d = d->next) {
				if (d->start <= start && start < d->end) {
					const uint32_t copylen = minimum(d->end - start, s->end - start);
					start += copylen;
					found = 1;
					break;
				}
			}

			if (!found) {
				break;
			}
		}

		if (start != s->end) {
			if (force == 0) {
				ERR("region [0x%08X:0x%08X] is not completely contained in any destination region", s->start, s->end);
				return -1;
			} else {
				WARN("region [0x%08X:0x%08X] is not completely contained in any destination region", s->start, s->end);
			}
		}
	}
	return 0;
}

static int parse_slice(char *str, struct region **ret, const struct stm8_part *const part) {
	struct region r = {0};

	if (*str != '[') {
		return -1;
	}

	str++;

	if (strncmp(str, "ram", 3) == 0) {
		r.start = part->ram_start;
		r.end = r.start + part->ram_size;
		str += 3;
	} else if (strncmp(str, "eeprom", 6) == 0) {
		r.start = part->eeprom_start;
		r.end = r.start + part->eeprom_size;
		str += 6;
	} else if (strncmp(str, "opt", 3) == 0) {
		r.start = part->opt_start;
		r.end = r.start + part->opt_size;
		str += 3;
	} else if (strncmp(str, "flash", 5) == 0) {
		r.start = part->flash_start;
		r.end = r.start + part->flash_size;
		str += 5;
	}

	int mode;
	if (*str == ':') {
		// start = none or named
	} else {
		if (*str == '-') {
			str++;
			mode = -1;
		} else if (*str == '+') {
			str++;
			mode = 1;
		} else {
			mode = 0;
		}

		const long int slice_start = strtol(str, &str, 0);

		if (errno == ERANGE) {
			// out of range
			return -1;
		}

		if (*str != ':') {
			// no :
			return -1;
		}

		if (mode == -1) {
			if (slice_start > r.end) {
				return -1;
			}

			r.start = r.end - slice_start;
		} else {
			r.start = r.start + slice_start;
		}
	}

	str++;

	if (*str == ']') {
		// end = none
	} else {
		if (*str == '-') {
			str++;
			mode = -1;
		} else if (*str == '+') {
			str++;
			mode = 1;
		} else {
			mode = 0;
		}

		const long int slice_end = strtol(str, &str, 0);

		if (errno == ERANGE) {
			// out of range
			ERR("out of range");
			return -1;
		}

		if (*str != ']') {
			// no ]
			ERR("no end");
			return -1;
		}

		if (mode == -1) {
			if (slice_end > r.end) {
				return -1;
			}
			r.end = r.end - slice_end;
		} else if (mode == 1) {
			r.end = r.start + slice_end;
		} else {
			r.end = slice_end;
		}
	}

	str++;

	if (*str != '\0') {
		// no \0
		return -1;
	}

	if (r.start > r.end) {
		ERR("inverted slice ");
		return -1;
	}

	return region_add_empty(ret, r.start, r.end - r.start);
}

static struct region *determine_all_regions(const struct stm8_part *const part) {
	struct region *r = NULL;
	if (region_add_empty(&r, part->ram_start, part->ram_size)) {
		ERR("failure");
		region_free(&r);
		return NULL;
	}
	if (region_add_empty(&r, part->eeprom_start, part->eeprom_size)) {
		ERR("failure");
		region_free(&r);
		return NULL;
	}
	if (region_add_empty(&r, part->flash_start, part->flash_size)) {
		ERR("failure");
		region_free(&r);
		return NULL;
	}
	if (region_add_empty(&r, part->opt_start, part->opt_size)) {
		ERR("failure");
		region_free(&r);
		return NULL;
	}
	return r;
}

static struct region *determine_regions(char *const slice, const struct stm8_part *const part) {
	struct region *r = NULL;
	if (strcmp(slice, "all") == 0) {
		r = determine_all_regions(part);
		if (!r) {
			ERR("failure");
			return NULL;
		}
	} else if (strcmp(slice, "[:]") == 0) {
		r = determine_all_regions(part);
		if (!r) {
			ERR("failure");
			return NULL;
		}
	} else if (strcmp(slice, "ram") == 0) {
		if (region_add_empty(&r, part->ram_start, part->ram_size)) {
			ERR("failure");
			return NULL;
		}
	} else if (strcmp(slice, "eeprom") == 0) {
		if (region_add_empty(&r, part->eeprom_start, part->eeprom_size)) {
			ERR("failure");
			return NULL;
		}
	} else if (strcmp(slice, "flash") == 0) {
		if (region_add_empty(&r, part->flash_start, part->flash_size)) {
			ERR("failure");
			return NULL;
		}
	} else if (strcmp(slice, "opt") == 0) {
		if (region_add_empty(&r, part->opt_start, part->opt_size)) {
			ERR("failure");
			return NULL;
		}
	} else if (parse_slice(slice, &r, part) == 0) {
		// ok
	} else {
		ERR("bad slice syntax");
		return NULL;
	}

	INFO("--- REGIONS ---");
	for (struct region *t = r; t; t = t->next) {
		INFO("REGION: [0x%08X:0x%08X]", t->start, t->end);
	}

	return r;
}

struct arguments {
	char *serialno;
	enum { MODE_NONE, MODE_READ, MODE_WRITE, MODE_VERIFY } mode;
	fileformat_t fmt;
	int force;
	int unlock;
	int lock;
	int autodetect_part;
	int skip_reset;
	uint8_t pad;
	char *arg[2];
	const struct stm8_part *part;
};

static int list_adapters(struct arguments *arguments) {
	libusb_device **devs;
	libusb_context *ctx = NULL;
	char vendor[32];
	char device[32];
	char serialno[32];
	char serialno_hex[64];

	int r = libusb_init(&ctx);
	if (r < 0) {
		ERR("libusb_init failed");
		return -1;
	}

	libusb_set_debug(ctx, STM8FLASH_LIBUSB_DEBUG_LEVEL);

	const int cnt = libusb_get_device_list(ctx, &devs);
	if (cnt < 0) {
		ERR("libusb_get_device_list failed");
		libusb_exit(ctx);
		return -1;
	}

	int numOfProgrammers = 0;
	// count available programmers
	for (int i = 0; i < cnt; i++) {
		struct libusb_device_descriptor desc;
		libusb_device_handle *tempHandle;
		libusb_get_device_descriptor(devs[i], &desc);

		if (gat_adapter_info(desc.idVendor, desc.idProduct)) {
			libusb_open(devs[i], &tempHandle);

			libusb_get_string_descriptor_ascii(tempHandle, desc.iManufacturer, (unsigned char *)vendor, sizeof(vendor));
			libusb_get_string_descriptor_ascii(tempHandle, desc.iProduct, (unsigned char *)device, sizeof(device));
			const int serialno_length = libusb_get_string_descriptor_ascii(tempHandle, desc.iSerialNumber, (unsigned char *)serialno, sizeof(serialno));
			serialno_to_hex(serialno, serialno_length, serialno_hex);

			// print programmer data if no serial number specified
			INFO("Programmer %d: %s %s, Serial Number:%.*s", numOfProgrammers, vendor, device, 2 * serialno_length, serialno_hex);
			if (!strncmp(serialno_hex, "303030303030303030303031", 2 * serialno_length)) {
				ERR("WARNING: Serial Number 303030303030303030303031 is most likely an invalid number reported due to a bug in older programmer firmware versions.");
			}
			libusb_close(tempHandle);
			numOfProgrammers++;
		}
	}

	libusb_free_device_list(devs, 1);
	libusb_exit(ctx);
	return 0;
}

static void list_supported_parts(struct arguments *arguments) {
	for (int i = 0; stm8_devices[i].name; i++) {
		fprintf(stderr, "%s ", stm8_devices[i].name);
	}
	fprintf(stderr, "\n");
}

static int parse_serial_number(struct arguments *arguments, char *arg) {
	const int len = strlen(arg);

	if (arguments->serialno) {
		ERR("Duplicate option -s");
		return EINVAL;
	}

	if ((len <= 0) || (len > 32 * 2) || (len % 2 != 0)) {
		ERR("Invalid serial number: %s", arg);
		return EINVAL;
	}

	arguments->serialno = arg;

	return 0;
}

static int parse_format(struct arguments *arguments, char *arg) {
	if (strcmp(arg, "ihex") == 0) {
		arguments->fmt = FORMAT_INTEL_HEX;
		return 0;
	}
	if (strcmp(arg, "bin") == 0) {
		arguments->fmt = FORMAT_BINARY;
		return 0;
	}
	if (strcmp(arg, "srec") == 0) {
		arguments->fmt = FORMAT_MOTOROLA_S_RECORD;
		return 0;
	}
	if (strcmp(arg, "auto") == 0) {
		arguments->fmt = FORMAT_AUTODETECT;
		return 0;
	}

	ERR("Invalid format `%s`", arg);
	return -1;
	// return ARGP_ERR_UNKNOWN;
}

static int parse_part(struct arguments *arguments, char *arg) {
	if (arguments->part) {
		ERR("Duplicate argument -p PART");
		return EINVAL;
	}

	arguments->part = get_part(arg);
	if (!arguments->part) {
		ERR("Unsupported part %s", arg);
		return EINVAL;
	}

	return 0;
}

void print_help(void) {
	const char help[] =
		"Usage: stm8flash [OPTION...] [SLICE] [FILE]\n"
		" This tool is used to read and write the various memories of the stm8 MCUs.\n"
		"\n"
		" FILE gives the file path where data is read/written. If FILE is not given"
		" stdin/stdout is used instead.\n"
		"\n"
		" SLICE Specifies the range of address that are to be read or written in"
		" [START:STOP] format. If START or STOP is preceded by `0x` it is taken as a"
		" hexidecimal number. The bytes at addresss START up to but not including STOP"
		" will be read/written. If START or STOP is ommited `start of file` and `end of"
		" file` is understood instead. If STOP is preceded by `+` it is interpreted"
		" relative to START and gives the number of bytes read/written.\n"
		" Alternatively you may specify `opt`, `eeprom`, `flash` or `ram`\n"
		"  -r            Read SLICE from MCU to FILE\n"
		"  -v            Read SLICE from MCU and compare to FILE\n"
		"  -w            Write SLICE from FILE to MCU\n"
		"  -u            Remove readout protection to unlock the MCU before other r/w operations.\n"
		"  -k            Apply readout protection to lock the MCU after other r/w operations.\n"
		"\n"
		"  -f            Skip the MCU size checks and force operation.\n"
		"  -n            Do not perform MCU reset after completion.\n"
		"  -p PART       Specify the part number or family.\n"
		"  -l            List all supported parts and exit.\n"
		"  -S SERIAL     Use a specific programmer by serial number.\n"
		"  -L            List all programmering adapers and exit.\n"
		"  -x FORMAT     Assume the FORMAT of FILE. Supported values are ihex, srec, bin.\n"
		"  -g            increase debug level.\n"
		"\n"
		"  -?            Give this help list\n"
		"  -V            Print program version\n"
		"\n"
		"Mandatory or optional arguments to long options are also mandatory or optional"
		"for any corresponding short options.\n"
		"\n"
		"The help text following options\n"
		"\n"
		"This is used after all other documentation; text is zero for this key\n"
		"\n"
		"Report bugs here https://github.com/schneidersoft/stm8flash.\n";
	fprintf(stderr, "%s", help);
}
void parse_args(struct arguments *arguments, int argc, char **argv) {
	struct xgetopt x = XGETOPT_INIT;
	int opt;
	while ((opt = xgetopt(&x, argc, argv, "hgukrwvatfnVlLp:S:x:")) != -1) {
		switch (opt) {
		case 'g':
			g_dbg_level = 2;
			break;
		case 'r':
			arguments->mode = MODE_READ;
			break;
		case 'u':
			arguments->unlock = 1;
			break;
		case 'k':
			arguments->lock = 1;
			break;
		case 'w':
			arguments->mode = MODE_WRITE;
			break;
		case 'v':
			arguments->mode = MODE_VERIFY;
			break;
		case 'p':
			if (parse_part(arguments, x.optarg)) {
				print_help();
				exit(-1);
			}
			break;
		case 'a':
			arguments->autodetect_part = 1;
			break;
		case 'S':
			parse_serial_number(arguments, x.optarg);
			break;
		case 'l':
			list_supported_parts(arguments);
			exit(0);
		case 'L':
			list_adapters(arguments);
			exit(0);
		case 'f':
			arguments->force = 1;
			break;
		case 'x':
			if (parse_format(arguments, x.optarg)) {
				print_help();
				exit(-1);
			}
			break;
		case 'n':
			arguments->skip_reset = 1;
			break;
		case 'V':
			fprintf(stderr, "version: %s\n", EXPAND_AND_QUOTE(GIT_VERSION));
			exit(0);
		case 'h':
			print_help();
			exit(0);
		default: /* '?' */
			print_help();
			exit(-1);
		}
	}

	if (x.optind < argc) {
		arguments->arg[0] = argv[x.optind++];
	}

	if (x.optind < argc) {
		arguments->arg[1] = argv[x.optind++];
	}

	if (x.optind < argc) {
		fprintf(stderr, "unsupported extra argument.\n");
		print_help();
		exit(-1);
	}
}

int main(int argc, char **argv) {
	struct arguments arguments = {0};
	arguments.mode = MODE_NONE;
	arguments.force = 0;
	arguments.fmt = FORMAT_AUTODETECT;

	parse_args(&arguments, argc, argv);

	const char *file = NULL;
	char *slice = NULL;
	if (arguments.arg[0] && arguments.arg[1]) {
		slice = arguments.arg[0];
		file = arguments.arg[1];
	} else {
		slice = "all";
		file = arguments.arg[0];
	}

	if (arguments.autodetect_part) {
		ERR("autodetection is not yet supported");
		return -1;
	}

	struct adapter *pgm = adapter_open(arguments.serialno);
	if (!pgm) {
		ERR("Couldn't initialize stlink");
		return -1;
	}

	DBG("DRIVER IS OPEN");

	const struct stm8_part *part = arguments.part;

	if (!part) {
		ERR("No part given");
		return -1;
	}

	if (stm8_open(pgm, part)) {
		ERR("Error communicating with MCU. Please check your SWIM connection.");
		return -1;
	}

	if (stm8_dump_regs(pgm, part)) {
		ERR("Error communicating with MCU. Please check your SWIM connection.");
		return -1;
	}

	if (arguments.unlock) {
		if (stm8_disable_rop(pgm, part)) {
			ERR("failed to disable readout protection");
			return -1;
		}
		INFO("UNLOCK SUCCESS (ROP)");
	}

	if (arguments.mode == MODE_READ) {
		if (!file) {
			ERR("no file given");
			return -1;
		}
		struct region *part_regions = determine_all_regions(part);
		if (!part_regions) {
			return -1;
		}
		// get requested region from slice and part info
		struct region *slice_regions = determine_regions(slice, part);
		if (!slice_regions) {
			ERR("unable to determine which regions to read");
			return -1;
		}

		// make sure regions specified in the file are availible in the chip
		if (check_regions(part_regions, slice_regions, arguments.force)) {
			ERR("failed to check regions");
			return -1;
		}

		// read them
		for (struct region *t = slice_regions; t; t = t->next) {
			if (aligned_read(pgm, part, t->b, t->start, t->end)) {
				ERR("Failed to read region");
				return -1;
			}
		}

		// dump the regions to output
		if (save_file(slice_regions, file, arguments.fmt)) {
			ERR("failed to save file");
			return -1;
		}

		INFO("SUCCESS");

	} else if (arguments.mode == MODE_VERIFY) {
		if (!file) {
			ERR("no file given");
			return -1;
		}
		struct region *part_regions = determine_all_regions(part);
		if (!part_regions) {
			return -1;
		}

		struct region *slice_regions = determine_regions(slice, part);
		if (!slice_regions) {
			ERR("unable to determine which regions to verify");
			return -1;
		}

		struct region *file_regions = NULL;
		if (load_from_file(&file_regions, file, &arguments.fmt)) {
			ERR("failed to load from file");
			return -1;
		}
		
		if (arguments.fmt == FORMAT_BINARY) {
			// binaries start at 0x0000. shift the regiouns so that it starts at the user provided region
			region_shift(file_regions, slice_regions->start);
		}

		// compute intersection
		struct region *read_region = region_intersection(slice_regions, file_regions);
		if (!read_region) {
			ERR("unable to determine intersections of regions to verify");
			return -1;
		}

		// make sure regions specified in the file are availible in the chip
		if (check_regions(part_regions, read_region, arguments.force)) {
			ERR("The provided regions are not availible on this part");
			return -1;
		}

		ERR("--- REGIONS ---");
		for (struct region *t = read_region; t; t = t->next) {
			ERR("REGION: [0x%08X:0x%08X]", t->start, t->end);
		}

		for (struct region *t = read_region; t; t = t->next) {
			uint8_t *scratch = malloc(t->blen);
			if (!scratch) {
				ERR("malloc");
				return -1;
			}
			assert(t->end - t->start == t->blen);
			if (aligned_read(pgm, part, scratch, t->start, t->end)) {
				ERR("Failed to read data");
				free(scratch);
				return -1;
			}

			if (memcmp(scratch, t->b, t->end - t->start) != 0) {
				ERR("Failed to verify region at 0x%04X", t->start);
				free(scratch);
				return -1;
			}
			free(scratch);
		}

		INFO("SUCCESS");

	} else if (arguments.mode == MODE_WRITE) {
		if (!file) {
			ERR("no file given");
			return -1;
		}
		struct region *part_regions = determine_all_regions(part);
		if (!part_regions) {
			return -1;
		}
		
		struct region *slice_regions = determine_regions(slice, part);
		if (!slice_regions) {
			ERR("unable to determine which regions to write");
			return -1;
		}

		struct region *file_regions = NULL;
		if (load_from_file(&file_regions, file, &arguments.fmt)) {
			ERR("failed to load from file");
			return -1;
		}

		if (arguments.fmt == FORMAT_BINARY) {
			DBG("shifting binary input to 0x%08X", slice_regions->start);
			// binaries start at 0x0000. shift the regiouns so that it starts at the user provided region
			region_shift(file_regions, slice_regions->start);
		}
		
		INFO("--- FILE REGIONS ---");
		for (struct region *t = file_regions; t; t = t->next) {
			INFO("REGION: [0x%08X:0x%08X]", t->start, t->end);
		}
		
		// compute intersection
		struct region *write_regions = region_intersection(slice_regions, file_regions);
		if (!write_regions) {
			ERR("unable to determine which regions to write");
			return -1;
		}

		// make sure regions specified in the file are availible in the chip
		if (check_regions(part_regions, write_regions, arguments.force)) {
			ERR("failed to check regions");
			return -1;
		}

		INFO("--- REGIONS ---");
		for (struct region *t = write_regions; t; t = t->next) {
			INFO("REGION: [0x%08X:0x%08X]", t->start, t->end);
		}

		for (struct region *t = write_regions; t; t = t->next) {
			if (aligned_write(pgm, part, t->b, t->start, t->end)) {
				ERR("Failed to write");
				return -1;
			}
		}

		INFO("SUCCESS");
	}

	if (arguments.lock) {
		if (stm8_enable_rop(pgm, part)) {
			ERR("failed to enable readout protection");
			return -1;
		}
		INFO("LOCK SUCCESS (ROP)");
	}
	
	if (!arguments.skip_reset) {
		if (stm8_reset(pgm, part)) {
			ERR("failed to perform MCU reset");
			return -1;
		}
	}

	return 0;
}
