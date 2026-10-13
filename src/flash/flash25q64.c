#include <string.h>
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "flash25q64.h"

#define SPIF SPI1

// SPI1 hangs off APB2 (84 MHz). BR=2 -> /8 -> 10.5 MHz: plenty for a full
// speed USB link (~1 MB/s) and far below what the plain READ DATA (03h)
// command tolerates (50 MHz). 42 MHz (BR=0) over short wires produced garbage.
#ifndef FLASH25Q64_SPI_BR
#define FLASH25Q64_SPI_BR 2u
#endif

enum {
	CMD_WRITE_ENABLE  = 0x06,
	CMD_READ_STATUS1  = 0x05,
	CMD_READ_STATUS2  = 0x35,
	CMD_WRITE_STATUS1 = 0x01,
	CMD_READ_DATA     = 0x03,
	CMD_PAGE_PROGRAM  = 0x02,
	CMD_SECTOR_ERASE  = 0x20,
	CMD_CHIP_ERASE    = 0xC7,
	CMD_JEDEC_ID      = 0x9F,
	CMD_RELEASE_PD    = 0xAB,
};

#define SR1_BUSY 0x01u
#define SR1_WEL  0x02u
#define SR1_BP   0x7Cu  // BP0..2, TB, SEC: block protection bits

// Datasheet maximums with headroom: tPP 3 ms, tSE 400 ms, tW 15 ms.
#define TMO_PROGRAM_MS 20u
#define TMO_ERASE_MS   1000u
#define TMO_STATUS_MS  50u

static uint8_t scratch[FLASH25Q64_SECTOR_SIZE];

static void spin(uint32_t n) {
	while (n--) __NOP();
}

static void cs_low(void) {
	PIN_CLR(GPIOA, GPIO_PIN(4));
	spin(2);
}

static void cs_high(void) {
	while (SPI_BSY(SPIF));
	PIN_SET(GPIOA, GPIO_PIN(4));
	// tSHSL: /CS has to stay high for >= 50 ns before the next command or the
	// chip ignores it (WRITE ENABLE followed straight away by an erase, say).
	spin(16);
}

static uint8_t xfer(uint8_t out) {
	while (!SPI_TXE_READY(SPIF));
	SPIF->DR = out;
	while (!SPI_RXNE_READY(SPIF));
	return (uint8_t)SPIF->DR;
}

static void cmd_addr(uint8_t cmd, uint32_t addr) {
	xfer(cmd);
	xfer((uint8_t)(addr >> 16));
	xfer((uint8_t)(addr >> 8));
	xfer((uint8_t)addr);
}

static uint8_t read_reg(uint8_t cmd) {
	cs_low();
	xfer(cmd);
	uint8_t v = xfer(0xFF);
	cs_high();
	return v;
}

uint8_t flash25q64_read_status(void) {
	return read_reg(CMD_READ_STATUS1);
}

bool flash25q64_busy(void) {
	return (flash25q64_read_status() & SR1_BUSY) != 0;
}

static bool wait_ready(uint32_t timeout_ms) {
	uint32_t start = ticks;
	while (flash25q64_busy()) {
		if (ticks - start > timeout_ms) return false;
	}
	return true;
}

static bool write_enable(void) {
	cs_low();
	xfer(CMD_WRITE_ENABLE);
	cs_high();
	return (flash25q64_read_status() & SR1_WEL) != 0;
}

uint32_t flash25q64_read_id(void) {
	cs_low();
	xfer(CMD_JEDEC_ID);
	uint32_t id = (uint32_t)xfer(0xFF) << 16;
	id |= (uint32_t)xfer(0xFF) << 8;
	id |= xfer(0xFF);
	cs_high();
	return id;
}

// With block protection bits set every erase/program silently does nothing.
// Clear them, keeping the rest of SR1 and all of SR2 (QE bit) as they are.
static bool unprotect(void) {
	uint8_t sr1 = flash25q64_read_status();
	if ((sr1 & SR1_BP) == 0) return true;

	uint8_t sr2 = read_reg(CMD_READ_STATUS2);
	if (!write_enable()) return false;
	cs_low();
	xfer(CMD_WRITE_STATUS1);
	xfer(sr1 & (uint8_t)~SR1_BP);
	xfer(sr2);
	cs_high();
	if (!wait_ready(TMO_STATUS_MS)) return false;
	return (flash25q64_read_status() & SR1_BP) == 0;
}

bool flash25q64_init(void) {
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

	// /CS: output, idle high, fast edges so it keeps up with SCK
	PIN_SET(GPIOA, GPIO_PIN(4));
	MODER(GPIOA, 4, 1);
	OSPEEDR(GPIOA, 4, 2);
	// SCK, MISO, MOSI: AF5, "high" speed (not "very high": less ringing on wires)
	for (uint32_t pin = 5; pin <= 7; pin++) {
		MODER(GPIOA, pin, 2);
		AFR(GPIOA, pin, 5);
		OSPEEDR(GPIOA, pin, 2);
	}
	PUPDR(GPIOA, 6, 1); // MISO pull-up: an absent chip reads 0xFF, not noise

	SPIF->CR1 = 0;
	SPIF->CR2 = 0;
	// Master, mode 0, 8-bit MSB first, software NSS held high
	SPIF->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (FLASH25Q64_SPI_BR << SPI_CR1_BR_Pos);
	SPIF->CR1 |= SPI_CR1_SPE;

	// Wake up in case it was put to sleep; harmless otherwise. tRES1 = 3 us.
	cs_low();
	xfer(CMD_RELEASE_PD);
	cs_high();
	delay_ms(1);

	if (flash25q64_read_id() != FLASH25Q64_JEDEC_ID) return false;

	// A chip that refuses to drop its protection bits is still readable; writes
	// will fail verification and light the error pattern instead.
	(void) unprotect();
	return true;
}

void flash25q64_read(uint32_t addr, uint8_t* dst, uint32_t len) {
	if (len == 0) return;
	cs_low();
	cmd_addr(CMD_READ_DATA, addr);
	while (len--) *dst++ = xfer(0xFF);
	cs_high();
}

bool flash25q64_sector_erase(uint32_t addr) {
	if (!write_enable()) return false;
	cs_low();
	cmd_addr(CMD_SECTOR_ERASE, addr & ~(FLASH25Q64_SECTOR_SIZE - 1));
	cs_high();
	return wait_ready(TMO_ERASE_MS);
}

// `len` must not cross a page boundary.
static bool program_page(uint32_t addr, const uint8_t* src, uint32_t len) {
	if (!write_enable()) return false;
	cs_low();
	cmd_addr(CMD_PAGE_PROGRAM, addr);
	while (len--) xfer(*src++);
	cs_high();
	return wait_ready(TMO_PROGRAM_MS);
}

bool flash25q64_program(uint32_t addr, const uint8_t* src, uint32_t len) {
	while (len) {
		uint32_t room = FLASH25Q64_PAGE_SIZE - (addr % FLASH25Q64_PAGE_SIZE);
		uint32_t n = len < room ? len : room;
		if (!program_page(addr, src, n)) return false;
		addr += n;
		src += n;
		len -= n;
	}
	return true;
}

bool flash25q64_write_sector(uint32_t addr, const uint8_t* data) {
	addr &= ~(FLASH25Q64_SECTOR_SIZE - 1);
	flash25q64_read(addr, scratch, FLASH25Q64_SECTOR_SIZE);

	// Programming can only clear bits. Any bit that has to go 0 -> 1 means the
	// whole sector has to be erased first.
	bool same = true;
	bool need_erase = false;
	for (uint32_t i = 0; i < FLASH25Q64_SECTOR_SIZE; i++) {
		if (scratch[i] == data[i]) continue;
		same = false;
		if (data[i] & (uint8_t)~scratch[i]) {
			need_erase = true;
			break;
		}
	}
	if (same) return true;

	if (need_erase) {
		if (!flash25q64_sector_erase(addr)) return false;
		memset(scratch, 0xFF, sizeof scratch);
	}
	for (uint32_t off = 0; off < FLASH25Q64_SECTOR_SIZE; off += FLASH25Q64_PAGE_SIZE) {
		if (memcmp(scratch + off, data + off, FLASH25Q64_PAGE_SIZE) == 0) continue;
		if (!program_page(addr + off, data + off, FLASH25Q64_PAGE_SIZE)) return false;
	}

	flash25q64_read(addr, scratch, FLASH25Q64_SECTOR_SIZE);
	return memcmp(scratch, data, FLASH25Q64_SECTOR_SIZE) == 0;
}

bool flash25q64_chip_erase_start(void) {
	if (!write_enable()) return false;
	cs_low();
	xfer(CMD_CHIP_ERASE);
	cs_high();
	return true;
}
