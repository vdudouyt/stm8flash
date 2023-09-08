
#include "stm8.h"
#include "swim.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "adapter.h"
#include "error.h"

#define STM8_REG_A    0x7F00
#define STM8_REG_PCE  0x7F01
#define STM8_REG_PCH  0x7F02
#define STM8_REG_PCL  0x7F03
#define STM8_REG_XH   0x7F04
#define STM8_REG_XL   0x7F05
#define STM8_REG_YH   0x7F06
#define STM8_REG_YL   0x7F07
#define STM8_REG_SPH  0x7F08
#define STM8_REG_SPL  0x7F09
#define STM8_REG_CC   0x7F0A

static inline int swim_write_byte(struct adapter *adp, uint8_t byte, uint32_t addr) {
	return adp->swim_wotf(adp, &byte, 1, addr);
}

static inline int swim_read_byte(struct adapter *adp, uint32_t addr, uint8_t *byte) {
	return adp->swim_rotf(adp, addr, byte, 1);
}

static inline int swim_write_block(struct adapter *adp, const uint8_t *byte, uint32_t length, uint32_t addr) {
	return adp->swim_wotf(adp, byte, length, addr);
}

static inline int swim_read_block(struct adapter *adp, uint32_t addr, uint8_t *byte, uint32_t length) {
	return adp->swim_rotf(adp, addr, byte, length);
}

int stm8_dump_regs(struct adapter *adp, const struct stm8_part *device) {
	uint8_t buf[13];
	if (adp->swim_rotf(adp, STM8_REG_A, buf, 11)) {
		// error
		ERR("SWIM READ FAILED");
		return -1;
	}
	
	if (adp->swim_rotf(adp, SWIM_CSR, &buf[11], 1)) {
		// error
		ERR("SWIM READ FAILED");
		return -1;
	}
	
	
	if (adp->swim_rotf(adp, SWIM_DM_CSR2, &buf[12], 1)) {
		// error
		ERR("SWIM READ FAILED");
		return -1;
	}
	
	DBG("STM8 REGISTES:");
	DBG("A   0x%02X", buf[0]);
	DBG("PCE 0x%02X", buf[1]);
	DBG("PCH 0x%02X", buf[2]);
	DBG("PCL 0x%02X", buf[3]);
	DBG("XH  0x%02X", buf[4]);
	DBG("XL  0x%02X", buf[5]);
	DBG("YH  0x%02X", buf[6]);
	DBG("YL  0x%02X", buf[7]);
	DBG("SPH 0x%02X", buf[8]);
	DBG("SPL 0x%02X", buf[9]);
	DBG("CC  0x%02X", buf[10]);
	DBG("SWIM_CSR      0x%02X", buf[11]);
	DBG("SWIM_DM_CSR2  0x%02X", buf[12]);
	return 0;
}

int stm8_reset(struct adapter *adp, const struct stm8_part *device) {
	// SWIM srst only works when the device is in debug mode
	uint8_t csr;
	if (adp->swim_rotf(adp, SWIM_CSR, &csr, 1)) {
		ERR("failed to get SIM_CSR");
		return -1;
	}
	
	DBG("SWIM_CSR: 0x%02X");
	if ((csr & SWIM_CSR_SWIM_DM) == 0) {
		ERR("SWIM NOT IN DEBUG MODE");
		return -1;
	}
	
	// disable swim after reset
	csr |= SWIM_CSR_RST;
	
	// set the rst flag so that we exit reset swim
	if (adp->swim_wotf(adp, &csr, 1, SWIM_CSR)) {
		ERR("failed to set SIM_CSR");
		return -1;
	}
	
	// clear the stall bit
	csr = 0;
	if (adp->swim_wotf(adp, &csr, 1, SWIM_DM_CSR2)) {
		ERR("failed to set SWIM_DM_CSR2");
		return -1;
	}

	if (adp->swim_srst(adp)) {
		ERR("FAIL");
		return -1;
	}
	usleep(1000);
	return 0;
}

static int wait_for_eop(struct adapter *adp, const struct stm8_part *device) {
	DBG("----!WAIT FOR EOP!----");
	// Wait for EOP to be set in FLASH_IAPSR
	// t_prog per the datasheets is 6ms typ, 6.6ms max, fast mode is twice as fast
	// usleep(prgmode == 0x10 ? 3000 : 6000);
	usleep(6000);
	// provide a better error message than 'tries exceeded' ?
	int retries = 5;
	while (retries > 0) {
		uint8_t iapsr;
		if (swim_read_byte(adp, device->FLASH_IAPSR, &iapsr)) {
			ERR("FAIL");
			return -1;
		}
		DBG("IAPSR: 0x%02X", iapsr);
		if (iapsr & 0x04) {
			break;
		}
		if (iapsr & 0x01) {
			ERR("target page is write protected (UBC) or read-out protection is enabled\n");
			return -1;
		}
		retries--;
		usleep(1000);
	}
	if (retries == 0) {
		ERR("tries exceeded");
		return -1;
	}
	return 0;
}

int stm8_write_block(struct adapter *const adp, const struct stm8_part *const device, const uint8_t *buf, uint32_t length, uint32_t addr, memtype_t memtype, int fast) {
	assert(addr % device->flash_block_size == 0);
	assert(length == device->flash_block_size);

	// flast clock
	if (swim_write_byte(adp, 0x00, device->CLK_CKDIVR)) {
		ERR("FAIL");
		exit(-1);
	}

	DBG("have fast clock\n\n\n");

	// Stall the cpu before doing any block programming
	uint8_t csr;
	if (swim_read_byte(adp, SWIM_DM_CSR2, &csr)) {
		DBG("UNABLE TO READ CSR");
		return -1;
	}
	if (swim_write_byte(adp, csr | 8, SWIM_DM_CSR2)) {
		DBG("UNABLE TO WRITE CSR");
		return -1;
	}

	INFO("PROGRESS [0x%08X: 0x%08X]", addr, addr + device->flash_block_size);

	// Set programming mode
	if (memtype == MEM_FLASH || memtype == MEM_EEPROM) {
		const uint8_t prgmode = fast ? 0x10 : 0x01;
		if (prgmode == 0x10) {
			INFO("FAST BB0CK PROG");
		} else {
			INFO("STANDARD BB0CK PROG");
		}

		if (swim_write_byte(adp, prgmode, device->FLASH_CR2)) {
			ERR("FAIL");
			exit(-1);
		}
		if (device->FLASH_NCR2 != 0) {
			if (swim_write_byte(adp, ~prgmode, device->FLASH_NCR2)) {
				ERR("FAIL");
				exit(-1);
			}
		}
	}

	// Unlock MASS
	if (memtype == MEM_FLASH) {
		DBG("write range: unlock FLASH");
		if (swim_write_byte(adp, 0x56, device->FLASH_PUKR)) {
			ERR("FAIL");
			exit(-1);
		}
		if (swim_write_byte(adp, 0xae, device->FLASH_PUKR)) {
			ERR("FAIL");
			exit(-1);
		}
	} else if (memtype == MEM_EEPROM || memtype == MEM_OPT) {
		DBG("write range: unlock EEPROM");
		if (swim_write_byte(adp, 0xae, device->FLASH_DUKR)) {
			ERR("FAIL");
			exit(-1);
		}
		if (swim_write_byte(adp, 0x56, device->FLASH_DUKR)) {
			ERR("FAIL");
			exit(-1);
		}
	}

	// block-based writing
	if (swim_write_block(adp, buf, device->flash_block_size, addr)) {
		// error
		ERR("SWIM WRITE FAILED");
		return -1;
	}

	if (memtype == MEM_FLASH || memtype == MEM_EEPROM) {
		if (wait_for_eop(adp, device)) {
			ERR("FAILED TO WAIT FOR EOP");
			return -1;
		}
	}

	if (memtype == MEM_FLASH || memtype == MEM_EEPROM || memtype == MEM_OPT) {
		// Reset DUL and PUL in IAPSR to disable flash and data writes.
		uint8_t iapsr;
		if (swim_read_byte(adp, device->FLASH_IAPSR, &iapsr)) {
			ERR("FAIL");
			exit(-1);
		}
		swim_write_byte(adp, iapsr & (~0x0a), device->FLASH_IAPSR);
	}

	return 0;
}
int stm8_read_block(struct adapter *const adp, const struct stm8_part *const part, uint32_t addr, uint8_t *buf, uint32_t length) {
	assert(addr <= 0x00FFFFFF);
	assert(addr % part->flash_block_size == 0);
	assert(length % part->flash_block_size == 0);

	return swim_read_block(adp, addr, buf, length);
}

int stm8_open(struct adapter *const adp, const struct stm8_part *const part) {
	// reset low
	
	// swim open...
	
	// set up regs
	
	// reset high
	// currently all done in swim_connect...

	return adp->swim_connect(adp);
}

int stm8_enable_rop(struct adapter *const adp, const struct stm8_part *const part) {
	// flast clock??? is it required?
	swim_write_byte(adp, 0x00, part->CLK_CKDIVR);

	// set the programming mode first! : RM0031 page 54 note 7
	DBG("OPT PROG");
	if (swim_write_byte(adp, 0x80, part->FLASH_CR2)) {
		ERR("FAIL");
		return -1;
	}
	if (part->FLASH_NCR2 != 0) {
		if (swim_write_byte(adp, 0x75, part->FLASH_NCR2)) {
			ERR("FAIL");
			return -1;
		}
	}

	// Then send the MASS keys
	DBG("unlock OPT");
	if (swim_write_byte(adp, 0xae, part->FLASH_DUKR)) {
		ERR("FAIL");
		return -1;
	}
	if (swim_write_byte(adp, 0x56, part->FLASH_DUKR)) {
		ERR("FAIL");
		return -1;
	}

	// finally lock the device by writing the ROP byte
	INFO("locking device.");
	if (part->rop_mode == ROP_AA_DIS) {
		swim_write_byte(adp, 0x00, 0x4800);
	} else if (part->rop_mode == ROP_AA_EN) {
		swim_write_byte(adp, 0xAA, 0x4800);
	}

	// after the next reset the changes should take effect
	// the datasheet says a POR reset is required for some changes to take effect
	// we hope a simple software reset will do the trick (performed by main later)
	return 0;
}

int stm8_disable_rop(struct adapter *const adp, const struct stm8_part *const part) {
	// flast clock??? is it required?
	swim_write_byte(adp, 0x00, part->CLK_CKDIVR);

	// set the programming mode first! : RM0031 page 54 note 7
	DBG("OPT PROG");
	if (swim_write_byte(adp, 0x81, part->FLASH_CR2)) {
		ERR("FAIL");
		return -1;
	}
	if (part->FLASH_NCR2 != 0) {
		if (swim_write_byte(adp, 0x75, part->FLASH_NCR2)) {
			ERR("FAIL");
			return -1;
		}
	}

	usleep(500000);
	// Then send the MASS keys
	DBG("unlock OPT");
	if (swim_write_byte(adp, 0xae, part->FLASH_DUKR)) {
		ERR("FAIL");
		return -1;
	}
	if (swim_write_byte(adp, 0x56, part->FLASH_DUKR)) {
		ERR("FAIL");
		return -1;
	}

	if (part->rop_mode == ROP_AA_DIS) {
		INFO("Unlocking device.");
		if (swim_write_byte(adp, 0xAA, 0x4800)) {
			ERR("FAIL");
			return -1;
		}

		// wait for EOP
		INFO("wait for EOP.");
		if (wait_for_eop(adp, part)) {
			ERR("FAIL");
			return -1;
		}

		INFO("UNL0CK AGAIN");
		if (swim_write_byte(adp, 0xAA, 0x4800)) {
			ERR("FAIL");
			return -1;
		}

		INFO("wait for EOP.");
		if (wait_for_eop(adp, part)) {
			ERR("FAIL");
			return -1;
		}
	} else if (part->rop_mode == ROP_AA_EN) {
		INFO("Unlocking device.");
		if (swim_write_byte(adp, 0x00, 0x4800)) {
			ERR("FAIL");
			return -1;
		}

		// wait for EOP
		INFO("wait for EOP.");
		if (wait_for_eop(adp, part)) {
			ERR("FAIL");
			return -1;
		}

		INFO("UNL0CK AGAIN");
		if (swim_write_byte(adp, 0x00, 0x4800)) {
			ERR("FAIL");
			return -1;
		}

		INFO("wait for EOP.");
		if (wait_for_eop(adp, part)) {
			ERR("FAIL");
			return -1;
		}
	}

	// after the next reset the changes should take effect
	// the datasheet says a POR reset is required for some changes to take effect
	// we hope a simple software reset will do the trick (performed by main later)
	return 0;
}

