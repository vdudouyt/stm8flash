#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "autodetect.h"
#include "byte_utils.h"
#include "utils.h"
  
#define SWIM_PCE      0x7F01
#define SWIM_PCH      0x7F02
#define SWIM_PCL      0x7F03
#define SWIM_SPH      0x7F08
#define SWIM_SPL      0x7F09
#define SWIM_CSR      0x7F80
#define DM_CSR2       0x7F99

#define ID_ADDRESS1   0x4FFC
#define ID_ADDRESS2   0x67F0
#define ID_ADDRESS3   0x67F1

#define FLASH_START   0x8000
#define BOOTROM_START 0x6000
#define RAM_START     0x0000

typedef enum {
  REGSET_STM8S,
  REGSET_STM8L
} regset_t;

static const stm8_regs_t stm8_regs[] = {REGS_STM8S, REGS_STM8L};

static unsigned char buffer[10];

typedef struct {
  uint16_t type_id_address;
  uint32_t type_id_value;
  const char *type_name;
  uint32_t ram_size;
  uint32_t flash_min_size;
  uint32_t flash_max_size;
  uint32_t flash_block_size;
  uint32_t eeprom_base_address;
  uint32_t eeprom_min_size;
  uint32_t eeprom_max_size;
  uint16_t unique_id_address;
  uint16_t unique_id_address_len;
  bool has_bootrom;
  ROP_type_t read_out_protection_mode;
  regset_t regset;

} stm8_autodetect_type_t;

static const stm8_autodetect_type_t stm8_autodetect_types[] = {
  {
    .type_id_address = 0x67F0,
    .type_id_value = 0x37394241,
    .type_name = "STM8AF/STM8S005",
    .ram_size = 2*1024,
    .flash_min_size = 16*1024,
    .flash_max_size = 32*1024,
    .flash_block_size = 128,
    .eeprom_base_address = 0x4000,
    .eeprom_min_size = 640,
    .eeprom_max_size = 1024,
    .unique_id_address = 0x48CD, // this type has no uniqueID, but will check this address to distinguish from STM8S105
    .unique_id_address_len = 0,
    .has_bootrom = true,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  {
    .type_id_address = 0x67F0,
    .type_id_value = 0x37394241,
    .type_name = "STM8S105",
    .ram_size = 2*1024,
    .flash_min_size = 16*1024,
    .flash_max_size = 32*1024,
    .flash_block_size = 128,
    .eeprom_base_address = 0x4000,
    .eeprom_min_size = 1024,
    .eeprom_max_size = 1024,
    .unique_id_address = 0x48CD,
    .unique_id_address_len = 12,
    .has_bootrom = true,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  {  // anyone a datasheet for this type?
    .type_id_address = 0x67F0,
    .type_id_value = 0x37394341,
    .type_name = "STM8AF51/STM8AH51",
    .ram_size = 6*1024,
    .flash_min_size = 128*1024,
    .flash_max_size = 128*1024,
    .flash_block_size = 128,
    .eeprom_base_address = 0x4000, 
    .eeprom_min_size = 0,
    .eeprom_max_size = 0xFFFF,
    .unique_id_address = 0,
    .unique_id_address_len = 0,
    .has_bootrom = true,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  {  // anyone a datasheet for this type?
    .type_id_address = 0x67F0,
    .type_id_value = 0x37394341,
    .type_name = "STM8AF51/STM8AH51",
    .ram_size = 12*1024,
    .flash_min_size = 256*1024,
    .flash_max_size = 256*1024,
    .flash_block_size = 128,
    .eeprom_base_address = 0x4000, 
    .eeprom_min_size = 0,
    .eeprom_max_size = 0xFFFF,
    .unique_id_address = 0,
    .unique_id_address_len = 0,
    .has_bootrom = true,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  {
    .type_id_address = 0x67F0,
    .type_id_value = 0x79314141,
    .type_name = "STLUX",
    .ram_size = 2*1024,
    .flash_min_size = 32*1024,
    .flash_max_size = 32*1024,
    .flash_block_size = 128,
    .eeprom_base_address = 0x4000, 
    .eeprom_min_size = 1024,
    .eeprom_max_size = 1024,
    .unique_id_address = 48E0,
    .unique_id_address_len = 8,
    .has_bootrom = true,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  {
    .type_id_address = 0x67F0,
    .type_id_value = 0x79314141,
    .type_name = "STNRG",
    .ram_size = 6*1024,
    .flash_min_size = 32*1024,
    .flash_max_size = 32*1024,
    .flash_block_size = 128,
    .eeprom_base_address = 0x4000, 
    .eeprom_min_size = 1024,
    .eeprom_max_size = 1024,
    .unique_id_address = 48E0,
    .unique_id_address_len = 8,
    .has_bootrom = true,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  { // anyone a datasheet for this type?
    .type_id_address = 0x67F1,
    .type_id_value = 0x55576588,
    .type_name = "STMAF51/STMAH51 32k",
    .ram_size = 2*1024,
    .flash_min_size = 32*1024,
    .flash_max_size = 32*1024,
    .flash_block_size = 128,
    .eeprom_base_address = 0x4000, 
    .eeprom_min_size = 0,
    .eeprom_max_size = 0xFFFF,
    .unique_id_address = 0,
    .unique_id_address_len = 0,
    .has_bootrom = true,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  { // anyone a datasheet for this type?
    .type_id_address = 0x67F1,
    .type_id_value = 0x55576588,
    .type_name = "STMAF51/STMAH51 48k",
    .ram_size = 3*1024,
    .flash_min_size = 48*1024,
    .flash_max_size = 48*1024,
    .flash_block_size = 128,
    .eeprom_base_address = 0x4000, 
    .eeprom_min_size = 0,
    .eeprom_max_size = 0xFFFF,
    .unique_id_address = 0,
    .unique_id_address_len = 0,
    .has_bootrom = true,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  { // anyone a datasheet for this type?
    .type_id_address = 0x67F1,
    .type_id_value = 0x55576588,
    .type_name = "STMAF51/STMAH51 64k",
    .ram_size = 4*1024,
    .flash_min_size = 64*1024,
    .flash_max_size = 64*1024,
    .flash_block_size = 128,
    .eeprom_base_address = 0x4000, 
    .eeprom_min_size = 0,
    .eeprom_max_size = 0xFFFF,
    .unique_id_address = 0,
    .unique_id_address_len = 0,
    .has_bootrom = true,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  { // anyone a datasheet for this type?
    .type_id_address = 0x67F1,
    .type_id_value = 0x55576588,
    .type_name = "STMAF52/STMAF62",
    .ram_size = 6*1024,
    .flash_min_size = 64*1024,
    .flash_max_size = 128*1024,
    .flash_block_size = 128,
    .eeprom_base_address = 0x4000, 
    .eeprom_min_size = 1024,
    .eeprom_max_size = 2048,
    .unique_id_address = 0x48CD, // this type has no uniqueID, but will check this address to distinguish from STM8S208
    .unique_id_address_len = 0,
    .has_bootrom = true,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  {
    .type_id_address = 0x67F1,
    .type_id_value = 0x55576588,
    .type_name = "STM8S208",
    .ram_size = 6*1024,
    .flash_min_size = 64*1024,
    .flash_max_size = 128*1024,
    .flash_block_size = 128,
    .eeprom_base_address = 0x4000,
    .eeprom_min_size = 1024,
    .eeprom_max_size = 2048,
    .unique_id_address = 0x48CD,
    .unique_id_address_len = 12,
    .has_bootrom = true,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  {
    .type_id_address = 0x4FFC,
    .type_id_value = 0x67581000,
    .type_name = "STM8Lx5",
    .ram_size = 1*1024,
    .flash_min_size = 8*1024,
    .flash_max_size = 8*1024,
    .flash_block_size = 64,
    .eeprom_base_address = 0x1000,
    .eeprom_min_size = 256,
    .eeprom_max_size = 256,
    .unique_id_address = 0,
    .unique_id_address_len = 0,
    .has_bootrom = true,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  {
    .type_id_address = 0x4FFC,
    .type_id_value = 0x67611000,
    .type_name = "STM8Lx01/STM8AL30xx",
    .ram_size = 1*1024 + 512, // 1.5k
    .flash_min_size = 4*1024,
    .flash_max_size = 8*1024,
    .flash_block_size = 64,
    .eeprom_base_address = 0x0, // part of flash
    .eeprom_min_size = 0,
    .eeprom_max_size = 0,
    .unique_id_address = 0x4925,
    .unique_id_address_len = 6,
    .has_bootrom = false,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  {
    .type_id_address = 0x4FFC,
    .type_id_value = 0x67641000,
    .type_name = "STM8AL31/STM8AL3L/STM8L151",
    .ram_size = 2*1024,
    .flash_min_size = 8*1024,
    .flash_max_size = 64*1024,
    .flash_block_size = 128,
    .eeprom_base_address = 0x1000,
    .eeprom_min_size = 1024,
    .eeprom_max_size = 2048,
    .unique_id_address = 0x4926,
    .unique_id_address_len = 6,
    .has_bootrom = true,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  {
    .type_id_address = 0x4FFC,
    .type_id_value = 0x67671000,
    .type_name = "STM8Sx03",
    .ram_size = 1*1024,
    .flash_min_size = 4*1024,
    .flash_max_size = 8*1024,
    .flash_block_size = 64,
    .eeprom_base_address = 0x4000,
    .eeprom_min_size = 640,
    .eeprom_max_size = 640,
    .unique_id_address = 0x4865,
    .unique_id_address_len = 12,
    .has_bootrom = false,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  {
    .type_id_address = 0x4FFC,
    .type_id_value = 0x67681000,
    .type_name = "STM8L15x",
    .ram_size = 2*1024,
    .flash_min_size = 32*1024,
    .flash_max_size = 64*1024,
    .flash_block_size = 128,
    .eeprom_base_address = 0x1000,
    .eeprom_min_size = 1024,
    .eeprom_max_size = 1024,
    .unique_id_address = 0x4926,
    .unique_id_address_len = 6,
    .has_bootrom = true,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  {
    .type_id_address = 0x4FFC,
    .type_id_value = 0x67691000,
    .type_name = "STM8TL5",
    .ram_size = 4*1024,
    .flash_min_size = 16*1024,
    .flash_max_size = 16*1024,
    .flash_block_size = 64,
    .eeprom_base_address = 0, //part of flash
    .eeprom_min_size = 0,
    .eeprom_max_size = 0,
    .unique_id_address = 0x4925,
    .unique_id_address_len = 6,
    .has_bootrom = false,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
  {
    .type_id_address = 0x4FFC,
    .type_id_value = 0x67991000,
    .type_name = "STM8AF62",
    .ram_size = 1*1024,
    .flash_min_size = 4*1024,
    .flash_max_size = 8*1024,
    .flash_block_size = 64,
    .eeprom_base_address = 0x4000,
    .eeprom_min_size = 640,
    .eeprom_max_size = 640,
    .unique_id_address = 0x4865,
    .unique_id_address_len = 12,
    .has_bootrom = false,
    .read_out_protection_mode = ROP_STM8S,
    .regset = REGSET_STM8S
  },
};

// find a match from the table above, starting from index
// returns number of matches, and updates index with first match
static uint8_t find_type (uint16_t id_address, uint32_t id_value, uint32_t id_mask, uint32_t ram_size, 
                      uint8_t *index) 
{
  uint8_t num_types = sizeof(stm8_autodetect_types)/sizeof(stm8_autodetect_type_t);
  uint8_t num_matches = 0;
  uint8_t start_index = *index;

  for (uint8_t i=start_index;i<num_types;i++) {
    if ((id_address == stm8_autodetect_types[i].type_id_address) &&
        ((id_value & id_mask) == (stm8_autodetect_types[i].type_id_value & id_mask)) &&
        (ram_size == stm8_autodetect_types[i].ram_size))
        {
          if (!num_matches)
            *index = i; // pass the first match
          num_matches++;
        }
  }
  return num_matches;
}

int autodetect(programmer_t *pgm, stm8_device_t *autodetect_device)
{
  uint32_t ram_size;
  const stm8_autodetect_type_t *stm8_type;
  uint32_t u32; // a 4-byte value
  bool has_bootrom = false;

  DEBUG_PRINT("starting autodetection\n");

  // assume we can perform read_range with device==NULL
  // purpose is to detect the device type, and derive the part specific registers
  // is ok for current stlink/espstlink drivers

  // first check for read-out protection
  pgm->read_range(pgm,NULL,buffer, FLASH_START, 4);
  u32 = load_int(buffer, 4, MP_BIG_ENDIAN);
  DEBUG_PRINT("reading 0x%x at flast start address : ",u32);
  if (u32 == 0x71717171) {
    fprintf(stderr, "active read-out protection prevents device auto-detection\n");
    return -1;
  }
  else {
    DEBUG_PRINT("no read-out protection active. proceeding with autodetection\n");
  }

  autodetect_device->name = NULL;

  // this assumes that the driver has stalled the CPU on ->open()
  // otherwise the PC/SPL could have moved while SWIM was activated

  // determine ram size -> read SPH/SPL (stack pointer)
  pgm->read_range(pgm,NULL,buffer, SWIM_SPH, 2);
  ram_size = load_int(buffer, 2, MP_BIG_ENDIAN);
  ram_size += 1;
  fprintf(stderr, "found SP = 0x%x, assuming ram size = %dK\n",(ram_size-1), ram_size/1024);

  // reading a few bytes at bootrom address BOOTROM_START = 0x6000
  pgm->read_range(pgm,NULL,buffer, BOOTROM_START, 4);
  u32 = load_int(buffer, 4, MP_BIG_ENDIAN);
  DEBUG_PRINT("reading 0x%x at bootrom start address : ",u32);
  if (u32 == 0x71717171) {
    has_bootrom = false;
    fprintf(stderr,"bootrom not present\n");
  }
  else {
    has_bootrom = true;
    fprintf(stderr,"bootrom is present\n");
  }

  // reading PC
  pgm->read_range(pgm,NULL,buffer, SWIM_PCE, 3);
  u32 = load_int(buffer, 3, MP_BIG_ENDIAN);
  fprintf(stderr, "found PC = 0x%x : ",u32);
  if (u32 >= FLASH_START) {
    if (has_bootrom)
      fprintf(stderr,"bootrom not active, ");
    fprintf(stderr,"mcu currently stalled in flash\n");
  }
  else if (u32 >= BOOTROM_START)
    fprintf(stderr,"mcu currently stalled in bootrom\n");

  uint16_t id_addresses[] = {ID_ADDRESS1, ID_ADDRESS2, ID_ADDRESS3};
  uint32_t id_masks[] = {0xFFFF0000, 0xFF00, 0xFFFF};
  for (uint8_t i=0; i<3;i++) { // loop the 3 possible ID address locations
    uint16_t id_address;
    uint32_t id_value, id_mask;
    uint8_t num_matches, find_index;
    bool match_ok;

    id_address = id_addresses[i];
    id_mask = id_masks[i];

    // reading id address
    pgm->read_range(pgm,NULL,buffer, id_address, 4);
    id_value = load_int(buffer, 4, MP_BIG_ENDIAN);
    DEBUG_PRINT("id @ %x = 0x%x\n",id_address, id_value);

    // check for matches in the lookup table
    find_index = 0;
    if (id_value != 0 && id_value != 0x71717171) {
      do {
        num_matches = find_type(id_address, id_value, id_mask, ram_size, &find_index);
        if (!num_matches)
          continue; // no matches found on id_address

        match_ok = true;
        stm8_type = &stm8_autodetect_types[find_index];
        find_index++; // prepare for next round
        DEBUG_PRINT("%d potential matches left\n", num_matches);
        DEBUG_PRINT("checking type %s\n",stm8_type->type_name);
        // check if match is plausible
        if (has_bootrom != stm8_type->has_bootrom) {
          match_ok = false;
          DEBUG_PRINT("-> no matching bootrom\n");
        }
        if (stm8_type->unique_id_address != 0) {
          // read a unique ID
          uint32_t uid;
          pgm->read_range(pgm,NULL,buffer, stm8_type->unique_id_address, 4);
          uid = load_int(buffer, 4, MP_BIG_ENDIAN);
          DEBUG_PRINT("uniqueID initial 4 bytes = 0x%x\n",uid);
          if ((stm8_type->unique_id_address_len) && ((uid == 0 || uid == 0x71717171))) {
            match_ok = false;
            DEBUG_PRINT("-> no valid unique ID\n");
          }
          if ((!stm8_type->unique_id_address_len) && (uid != 0) && (uid != 0x71717171)) {
            match_ok = false;
            DEBUG_PRINT("-> found unexpected unique ID\n");
          }
        }
        if (match_ok) {
          fprintf(stderr,"possible match! found %s with %d bytes RAM, flash size between %d and %d, "
            "eeprom at 0x%x with size between %d and %d, "
            "flash block size = %d\n",
            stm8_type->type_name, stm8_type->ram_size, stm8_type->flash_min_size, stm8_type->flash_max_size,
            stm8_type->eeprom_base_address, stm8_type->eeprom_min_size, stm8_type->eeprom_max_size,
            stm8_type->flash_block_size);

          if (!autodetect_device->name) {
            autodetect_device->name = stm8_type->type_name;
            autodetect_device->flash_size = stm8_type->flash_min_size;
            autodetect_device->flash_start = FLASH_START;
            autodetect_device->flash_block_size = stm8_type->flash_block_size;
            autodetect_device->eeprom_size = stm8_type->eeprom_min_size;
            autodetect_device->eeprom_start = stm8_type->eeprom_base_address;
            autodetect_device->ram_start = RAM_START;
            autodetect_device->ram_size = stm8_type->ram_size;
            autodetect_device->read_out_protection_mode = stm8_type->read_out_protection_mode;
            autodetect_device->regs = stm8_regs[stm8_type->regset];
          }
          else { // we have a new match, check for conflicting match
            if ((stm8_type->flash_block_size != autodetect_device->flash_block_size) ||
                ((stm8_type->eeprom_base_address != autodetect_device->eeprom_start))) {
              return -3;
            }
            else {
              // TODO : update autodetect_device to return the smallest flash size/eeprom size/ram size
            }
          }
        }
      } while (num_matches > 0);
    }
  } // for

  DEBUG_PRINT( "autodetection completed\n");

  if (!autodetect_device->name) {
    return -2; // no match found
  }

  return 0;
}
