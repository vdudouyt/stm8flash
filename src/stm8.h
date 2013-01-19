/* This header file contains the generic information
   about supported STM8 devices */

typedef struct stm8_mcu_spec_s {
	const char *name;
	unsigned int ram_start;
	unsigned int ram_size;
	unsigned int eeprom_start;
	unsigned int eeprom_size;
	unsigned int flash_start;
	unsigned int flash_size;
} stm8_mcu_spec_t;

stm8_mcu_spec_t parts[] = {
	{ "stm8s003", 0x0000, 1*2^10, 0x4000, 128, 0x8000, 8*2^10 },
	{ "stm8s105", 0x0000, 2*2^10, 0x4000, 2^10, 0x8000, 16*2^10 },
	{ "stm8l150", 0x0000, 2*2^10, 0x1000, 2^10, 0x8000, 32*2^10 },
	{ NULL },
};
