#ifndef __STM8_H
#define __STM8_H

#include "adapter.h"

typedef enum {
	MEM_UNKNOWN,
	MEM_RAM,
	MEM_EEPROM,
	MEM_FLASH,
	MEM_OPT,
} memtype_t;

typedef struct stm8_regs {
	unsigned int CLK_CKDIVR;
	unsigned int FLASH_PUKR;
	unsigned int FLASH_DUKR;
	unsigned int FLASH_IAPSR;
	unsigned int FLASH_CR2;
	unsigned int FLASH_NCR2;
	unsigned int FLASH_DM_CSR2;
} stm8_regs_t;

typedef enum {
	ROP_UNKNOWN,
	ROP_AA_EN,  // Disable ROP = 0x00 and reset. Option bytes are written noninverted and inverted.
	ROP_AA_DIS, // Disable ROP = 0xaa, read EOP, ROP = 0xaa, read EOP. Option bytes are written noninverted.
} ROP_type_t;

struct stm8_part {
	const char *name;
	unsigned int ram_start;
	unsigned int ram_size;
	unsigned int eeprom_start;
	unsigned int eeprom_size;
	unsigned int opt_start;
	unsigned int opt_size;
	unsigned int flash_start;
	unsigned int flash_size;
	unsigned int flash_block_size;
	unsigned int option_bytes_size;
	ROP_type_t rop_mode;
	unsigned int CLK_CKDIVR;
	unsigned int FLASH_PUKR;
	unsigned int FLASH_DUKR;
	unsigned int FLASH_IAPSR;
	unsigned int FLASH_CR2;
	unsigned int FLASH_NCR2;
	unsigned int FLASH_DM_CSR2;
};

extern const struct stm8_part stm8_devices[];

// int stm8_enable_rop(struct adapter *pgm, const stm8_device_t *dev);

// int stm8_disable_rop(struct adapter *pgm, const stm8_device_t *dev);

int stm8_write_block(struct adapter *const pgm, const struct stm8_part *const part, const uint8_t *buf, uint32_t length, uint32_t addr, memtype_t memtype, int fast);
int stm8_read_block(struct adapter *const pgm, const struct stm8_part *const part, uint32_t addr, uint8_t *buf, uint32_t length);
int stm8_open(struct adapter *const pgm, const struct stm8_part *const part);
int stm8_enable_rop(struct adapter *const pgm, const struct stm8_part *const part);
int stm8_disable_rop(struct adapter *const pgm, const struct stm8_part *const part);
int stm8_dump_regs(struct adapter *adp, const struct stm8_part *device);
int stm8_reset(struct adapter *adp, const struct stm8_part *device);
#endif
