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


typedef enum {
	ROP_UNKNOWN,
	ROP_STM8S,     // Disable ROP = 0x00 and reset. Option bytes are written noninverted and inverted.
	ROP_STM8L,     // Disable ROP = 0xaa, read EOP, ROP = 0xaa, read EOP. Option bytes are written noninverted.
} ROP_type_t;

typedef struct stm8_device {
	const char *name;
	unsigned int ram_start;
	unsigned int ram_size;
	unsigned int eeprom_start;
	unsigned int eeprom_size;
	unsigned int flash_start;
	unsigned int flash_size;
	unsigned int flash_block_size;
	unsigned int option_bytes_size;
	ROP_type_t   read_out_protection_mode; 
	stm8_regs_t regs;
} stm8_device_t;

extern const stm8_device_t stm8_devices[];

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
        .CLK_CKDIVR = 0x50c0,  \
        .FLASH_PUKR = 0x5052,  \
        .FLASH_DUKR = 0x5053,  \
        .FLASH_IAPSR = 0x5054, \
        .FLASH_CR2 = 0x5051,   \
        .FLASH_NCR2 = 0x0000,   \
}

#endif

