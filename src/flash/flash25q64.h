#pragma once

#include <stdint.h>
#include <stdbool.h>

// Winbond W25Q64JV (8 MiB) SPI NOR flash on SPI1
//   PA4 /CS (plain GPIO), PA5 SCK, PA6 MISO, PA7 MOSI (AF5)

#define FLASH25Q64_PAGE_SIZE    256u
#define FLASH25Q64_SECTOR_SIZE  4096u
#define FLASH25Q64_SIZE         (8u * 1024u * 1024u)
#define FLASH25Q64_JEDEC_ID     0xEF4017u

// Bring up SPI1 and the pins, wake the chip, verify the JEDEC ID and lift any
// block protection. Returns false when the chip does not answer as expected.
bool     flash25q64_init(void);
uint32_t flash25q64_read_id(void);
uint8_t  flash25q64_read_status(void);
bool     flash25q64_busy(void);

// Read `len` bytes starting at any byte address.
void flash25q64_read(uint32_t addr, uint8_t* dst, uint32_t len);

// Replace one 4 KiB sector (`addr` sector aligned, `data` is 4 KiB).
// Erases only when some bit has to go 0 -> 1, programs only pages that differ,
// and verifies the result by reading it back.
bool flash25q64_write_sector(uint32_t addr, const uint8_t* data);

bool flash25q64_sector_erase(uint32_t addr);
// Program `len` bytes at any address; the range must already be erased.
bool flash25q64_program(uint32_t addr, const uint8_t* src, uint32_t len);

// Kick off a full chip erase (tens of seconds) and return immediately.
// Poll flash25q64_busy() to find out when it is done.
bool flash25q64_chip_erase_start(void);
