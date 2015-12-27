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

const stm8_device_t stm8_devices[] = {
    {
        .name = "stlux???a",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8af526?",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8af528?",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 2048,
        .flash_start = 0x8000,
        .flash_size = 64*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8af52a?",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 2048,
        .flash_start = 0x8000,
        .flash_size = 128*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8af6213",
        .ram_start = 0x0000,
        .ram_size = 1*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 640,
        .flash_start = 0x8000,
        .flash_size = 4*1024,
        .flash_block_size = 64,
        REGS_STM8S
    },
    {
        .name = "stm8af6223",
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
        .name = "stm8af6223a",
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
        .name = "stm8af6226",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 640,
        .flash_start = 0x8000,
        .flash_size = 8*1024,
        .flash_block_size = 64,
        REGS_STM8S
    },
    {
        .name = "stm8af624?",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 512,
        .flash_start = 0x8000,
        .flash_size = 16*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8af6266",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8af6268",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8af6269",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8af628?",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 2048,
        .flash_start = 0x8000,
        .flash_size = 64*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8af62a?",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 2048,
        .flash_start = 0x8000,
        .flash_size = 128*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8al313?",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 8*1024,
        .flash_block_size = 128,
        REGS_STM8L
    },
    {
        .name = "stm8al314?",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 16*1024,
        .flash_block_size = 128,
        REGS_STM8L
    },
    {
        .name = "stm8al316?",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 128,
        REGS_STM8L
    },
    {
        .name = "stm8al3l4?",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 16*1024,
        .flash_block_size = 128,
        REGS_STM8L
    },
    {
        .name = "stm8al3l6?",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 128,
        REGS_STM8L
    },
    {
        .name = "stm8l051f3",
        .ram_start = 0x0000,
        .ram_size = 1*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 256,
        .flash_start = 0x8000,
        .flash_size = 8*1024,
        .flash_block_size = 64,
        REGS_STM8L
    },
    {
        .name = "stm8l052c6",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 256,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 64,
        REGS_STM8L
    },
    {
        .name = "stm8l052r8",
        .ram_start = 0x0000,
        .ram_size = 4*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 256,
        .flash_start = 0x8000,
        .flash_size = 64*1024,
        .flash_block_size = 64,
        REGS_STM8L
    },
    {
        .name = "stm8l101f1",
        .ram_start = 0x0000,
        .ram_size = 0x05FF,
        .eeprom_start = 0x9FFF, // Overlapping flash and eeprom
        .eeprom_size = 0,
        .flash_start = 0x8000,
        .flash_size = 2*1024,
        .flash_block_size = 64,
        REGS_STM8L
    },
    {
        .name = "stm8l101?2",
        .ram_start = 0x0000,
        .ram_size = 0x05FF,
        .eeprom_start = 0x9FFF, // Overlapping flash and eeprom
        .eeprom_size = 0,
        .flash_start = 0x8000,
        .flash_size = 4*1024,
        .flash_block_size = 64,
        REGS_STM8L
    },
    {
        .name = "stm8l101?3",
        .ram_start = 0x0000,
        .ram_size = 0x05FF,
        .eeprom_start = 0x9FFF, // Overlapping flash and eeprom
        .eeprom_size = 0,
        .flash_start = 0x8000,
        .flash_size = 8*1024,
        .flash_block_size = 64,
        REGS_STM8L
    },
    {
        .name = "stm8l151?2",
        .ram_start = 0x0000,
        .ram_size = 1*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 256,
        .flash_start = 0x8000,
        .flash_size = 4*1024,
        .flash_block_size = 64,
        REGS_STM8L
    },
    {
        .name = "stm8l151?3",
        .ram_start = 0x0000,
        .ram_size = 1*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 256,
        .flash_start = 0x8000,
        .flash_size = 8*1024,
        .flash_block_size = 64,
        REGS_STM8L
    },
    {
        .name = "stm8l151?4",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 16*1024,
        .flash_block_size = 64,
        REGS_STM8L
    },
    {
        .name = "stm8l151?6",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 128,
        REGS_STM8L
    },
    {
        .name = "stm8l151?8",
        .ram_start = 0x0000,
        .ram_size = 4*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 2048,
        .flash_start = 0x8000,
        .flash_size = 64*1024,
        .flash_block_size = 128,
        REGS_STM8L
    },
    {
        .name = "stm8l152?4",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 16*1024,
        .flash_block_size = 64,
        REGS_STM8L
    },
    {
        .name = "stm8l152?6",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 128,
        REGS_STM8L
    },
    {
        .name = "stm8l152?8",
        .ram_start = 0x0000,
        .ram_size = 4*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 2048,
        .flash_start = 0x8000,
        .flash_size = 64*1024,
        .flash_block_size = 128,
        REGS_STM8L
    },
    {
        .name = "stm8l162?8",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x1000,
        .eeprom_size = 2048,
        .flash_start = 0x8000,
        .flash_size = 64*1024,
        .flash_block_size = 128,
        REGS_STM8L
    },
    {
        .name = "stm8s003?3",
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
        .name = "stm8s005?6",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 128,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s007c8",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 128,
        .flash_start = 0x8000,
        .flash_size = 64*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s103f2",
        .ram_start = 0x0000,
        .ram_size = 1*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 640,
        .flash_start = 0x8000,
        .flash_size = 4*1024,
        .flash_block_size = 64,
        REGS_STM8S
    },
    {
        .name = "stm8s103?3",
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
        .name = "stm8s105?4",
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
        .name = "stm8s105?6",
        .ram_start = 0x0000,
        .ram_size = 2*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s207c8",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 1536,
        .flash_start = 0x8000,
        .flash_size = 64*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s207cb",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 2048,
        .flash_start = 0x8000,
        .flash_size = 128*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s207k8",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 64*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s207m8",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 2048,
        .flash_start = 0x8000,
        .flash_size = 64*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s207mb",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 2048,
        .flash_start = 0x8000,
        .flash_size = 128*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s207r8",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 1536,
        .flash_start = 0x8000,
        .flash_size = 64*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s207rb",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 2048,
        .flash_start = 0x8000,
        .flash_size = 128*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s207s8",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 1536,
        .flash_start = 0x8000,
        .flash_size = 64*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s207sb",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 1536,
        .flash_start = 0x8000,
        .flash_size = 128*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s207?6",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s208c6",
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
        .name = "stm8s208s6",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 1536,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s208?8",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 2048,
        .flash_start = 0x8000,
        .flash_size = 64*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s208?b",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 2048,
        .flash_start = 0x8000,
        .flash_size = 128*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8s903?3",
        .ram_start = 0x0000,
        .ram_size = 1*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 640,
        .flash_start = 0x8000,
        .flash_size = 8*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8splnb1",
        .ram_start = 0x0000,
        .ram_size = 1*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 640,
        .flash_start = 0x8000,
        .flash_size = 8*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    {
        .name = "stm8tl5??4",
        .ram_start = 0x0000,
        .ram_size = 4096,
        .eeprom_start = 0x9FFF, // Overlapping flash and eeprom
        .eeprom_size = 0,
        .flash_start = 0x8000,
        .flash_size = 16*1024,
        .flash_block_size = 64,
        REGS_STM8L
    },
    {
        .name = "stnrg???a",
        .ram_start = 0x0000,
        .ram_size = 6*1024,
        .eeprom_start = 0x4000,
        .eeprom_size = 1024,
        .flash_start = 0x8000,
        .flash_size = 32*1024,
        .flash_block_size = 128,
        REGS_STM8S
    },
    { NULL },
};

