#pragma once

#include <stdint.h>

// 25Q64JVSIQ 8 MiB SPI flash driver
// Uses SPI1 (PA4 NSS, PA5 SCK, PA6 MISO, PA7 MOSI)

#define FLASH25Q64_PAGE_SIZE   256
#define FLASH25Q64_SECTOR_SIZE 4096
#define FLASH25Q64_SIZE        (8 * 1024 * 1024)

void flash25q64_init(void);

void flash25q64_read(uint32_t addr, uint8_t* dst, uint32_t len);

// Erase before write. addr and len must be sector-aligned (4 KiB).
void flash25q64_erase_and_write(uint32_t addr, const uint8_t* src, uint32_t len);

// Program only; caller must erase sectors first. len may be arbitrary.
void flash25q64_program(uint32_t addr, const uint8_t* src, uint32_t len);

void flash25q64_sector_erase(uint32_t addr);
void flash25q64_chip_erase(void);

uint32_t flash25q64_read_id(void);
uint8_t  flash25q64_read_status(void);
void     flash25q64_wait_busy(void);
