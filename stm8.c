#include <stdio.h>
#include "stm8.h"

#define REGS_STM8S { \
	  	.CLK_CKDIVR = 0x50c6,  \
		.FLASH_PUKR = 0x5062,  \
		.FLASH_DUKR = 0x5064,  \
		.FLASH_IAPSR = 0x505f, \
		.FLASH_CR2 = 0x505b,   \
		.FLASH_NCR2 = 0x505c,   \
}

// Note: FLASH_NCR2 not present on stm8l
#define REGS_STM8L { \
		.CLK_CKDIVR = 0x50c6,  \
		.FLASH_PUKR = 0x5052,  \
		.FLASH_DUKR = 0x5053,  \
		.FLASH_IAPSR = 0x5054, \
		.FLASH_CR2 = 0x5051,   \
		.FLASH_NCR2 = 0x0000,   \
}

stm8_device_t stm8_devices[] = {
	{
	  .name = "stm8s003",
	  .ram_start = 0x0000,
	  .ram_size = 1*1024,
	  .eeprom_start = 0x4000,
	  .eeprom_size = 128,
	  .flash_start = 0x8000,
	  .flash_size = 8*1024,
      .flash_block_size = 64,
	  REGS_STM8S
	},
	{
	  .name = "stm8s103",
	  .ram_start = 0x0000,
	  .ram_size = 1*1024,
	  .eeprom_start = 0x4000,
	  .eeprom_size = 640,
	  .flash_start = 0x8000,
	  .flash_size = 8*1024,
      .flash_block_size = 64,
	  REGS_STM8S
	 },
	{
	  .name = "stm8s105",
	  .ram_start = 0x0000,
	  .ram_size = 2*1024,
	  .eeprom_start = 0x4000,
	  .eeprom_size = 1024,
	  .flash_start = 0x8000,
	  .flash_size = 16*1024,
      .flash_block_size = 128,
	  REGS_STM8S
	 },
	{
	  .name = "stm8s208",
	  .ram_start = 0x0000,
	  .ram_size = 6*1024,
	  .eeprom_start = 0x4000,
	  .eeprom_size = 2048,
	  .flash_start = 0x8000,
	  .flash_size = 32*1024,
      .flash_block_size = 128,
	  REGS_STM8S
	 },
	{
	  .name = "stm8l150",
	  .ram_start = 0x0000,
	  .ram_size = 2*1024,
	  .eeprom_start = 0x1000,
	  .eeprom_size = 1024,
	  .flash_start = 0x8000,
	  .flash_size = 32*1024,
      .flash_block_size = 128,
	  REGS_STM8L
	 },
	{ NULL },
};
