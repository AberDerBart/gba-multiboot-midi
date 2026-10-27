#include "rom.h"
#include <pico/stdio.h>
#include <pico/stdio_usb.h>
#include <hardware/flash.h>
#include <pico/flash.h>
#include <string.h>

#define TIMEOUT 100000

#define MAX_ROM_SIZE (256 * 1024)
#define MIN_ROM_SIZE 0xc0
#define FLASH_ROM_OFFSET (PICO_FLASH_SIZE_BYTES - 2 * FLASH_SECTOR_SIZE - MAX_ROM_SIZE)
#define FLASH_HEADER_OFFSET (FLASH_ROM_OFFSET - FLASH_SECTOR_SIZE)

#define MAGIC_NUMBER 0xfe5c37a4

typedef struct rom_header_t {
  uint32_t size;
  uint32_t magic_number;
} rom_header_t;


const rom_header_t* rom_header = (const rom_header_t*)(XIP_BASE + FLASH_HEADER_OFFSET);
const uint8_t* rom = (const uint8_t*)(XIP_BASE + FLASH_ROM_OFFSET);

uint32_t rom_get_size() {
  if (rom_header->magic_number != MAGIC_NUMBER) {
    return 0;
  }
  return rom_header->size;
}

bool stdio_read_uint32(uint32_t* dst) {
  uint32_t buf = 0;
  for (int i = 0; i < 4; i++) {
    int r = stdio_getchar_timeout_us(TIMEOUT);
    if (r != (0xff & r)) {
      return false;
    }
    buf |= (uint32_t)r << (i * 8);
  }
  *dst = buf;
  return true;
}

void flash_erase_header(void* _) {
  (void) _;
  flash_range_erase(FLASH_HEADER_OFFSET, FLASH_SECTOR_SIZE);
}

static uint8_t buffer[FLASH_SECTOR_SIZE];

void flash_write_sector(void* param) {
  uint32_t* page_offset = param;
  flash_range_erase(*page_offset, FLASH_SECTOR_SIZE);
  flash_range_program(*page_offset, buffer, FLASH_SECTOR_SIZE);
}

void flash_write_header(void* header) {
  memcpy(buffer, header, sizeof(rom_header_t));
  flash_range_program(FLASH_HEADER_OFFSET, buffer, FLASH_PAGE_SIZE);
}

bool rom_upload_stdio() {
  if (!stdio_usb_connected()) {
    return false;
  }

  rom_header_t header = {
    .size = 0,
    .magic_number = MAGIC_NUMBER,
  };

  if (!stdio_read_uint32(&header.size)) {
    return false;
  }
  
  if (header.size < MIN_ROM_SIZE || header.size > MAX_ROM_SIZE) {
    return false;
  }

  // erase header first, so if something goes wrong along the way, the rom is considered invalid
  if (flash_safe_execute(flash_erase_header, NULL, 100) != PICO_OK) {
    return false;
  }

  uint32_t remaining = header.size;


  // copy data
  uint32_t page_offset = FLASH_ROM_OFFSET;
  while (remaining > 0) {
    unsigned int i = 0;
    for (; i < FLASH_SECTOR_SIZE && i < remaining; i++) {
      int c = stdio_getchar_timeout_us(TIMEOUT);
      if (c != (0xff & c)) {
        return false;
      }
      buffer[i] = c & 0xff;
    }
    if (i < FLASH_SECTOR_SIZE) {
      memset(&buffer[i], 0, FLASH_SECTOR_SIZE-i);
    }
    if (flash_safe_execute(flash_write_sector, &page_offset, 100) != PICO_OK) {
      return false;
    }
    page_offset += FLASH_SECTOR_SIZE;
    remaining -= i;
  }

  // write header to flash
  memset(buffer, 0xff, FLASH_PAGE_SIZE);
  memcpy(buffer, &header, sizeof(header));
  if (flash_safe_execute(flash_write_header, &header, 100) != PICO_OK) {
    return false;
  }

  return true;
}

