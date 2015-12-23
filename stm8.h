#ifndef __STM8_H
#define __STM8_H

/* This header file contains the generic information
   about supported STM8 devices */

typedef struct stm8_regs {
	unsigned int CLK_CKDIVR;
	unsigned int FLASH_PUKR;
	unsigned int FLASH_DUKR;
	unsigned int FLASH_IAPSR;
	unsigned int FLASH_CR2;
	unsigned int FLASH_NCR2;
} stm8_regs_t;

typedef struct stm8_device {
	const char *name;
	unsigned int ram_start;
	unsigned int ram_size;
	unsigned int eeprom_start;
	unsigned int eeprom_size;
	unsigned int flash_start;
	unsigned int flash_size;
    unsigned int flash_block_size;
	stm8_regs_t regs;
} stm8_device_t;

extern const stm8_device_t stm8_devices[];

#endif
