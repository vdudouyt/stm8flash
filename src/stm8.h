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
