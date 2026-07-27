#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "flash25q64.h"

// 25Q64JVSIQ 8 MiB SPI flash driver
// Uses SPI1 (PA4 NSS, PA5 SCK, PA6 MISO, PA7 MOSI)

#define SPIF SPI1

DECLARE_SPI(FLASH_SCK , A, 5, 5);
DECLARE_SPI(FLASH_MISO, A, 6, 5);
DECLARE_SPI(FLASH_MOSI, A, 7, 5);
DECLARE_GPIO_MOUT(FLASH_NSS, A, 4);

// Commands
#define CMD_WRITE_ENABLE       0x06
#define CMD_WRITE_DISABLE      0x04
#define CMD_READ_STATUS_REG1   0x05
#define CMD_READ_STATUS_REG2   0x35
#define CMD_READ_STATUS_REG3   0x15
#define CMD_READ_DATA          0x03
#define CMD_PAGE_PROGRAM       0x02
#define CMD_SECTOR_ERASE       0x20
#define CMD_BLOCK_ERASE_32K    0x52
#define CMD_BLOCK_ERASE_64K    0xD8
#define CMD_CHIP_ERASE         0xC7
#define CMD_JEDEC_ID           0x9F
#define CMD_RELEASE_PD         0xAB

static void cs_low(void)  { FLASH_NSS_LOW(); }
static void cs_high(void) { FLASH_NSS_HIGH(); }

static uint8_t spi_xfer(uint8_t tx) {
	while (!SPI_TXE_READY(SPIF));
	SPIF->DR = tx;
	while (!SPI_RXNE_READY(SPIF));
	return (uint8_t)SPIF->DR;
}

static void spi_write(const uint8_t* data, uint32_t len) {
	for (uint32_t i = 0; i < len; i++) {
		(void)spi_xfer(data[i]);
	}
	while (SPI_BSY(SPIF));
}

static void spi_read(uint8_t* dst, uint32_t len) {
	for (uint32_t i = 0; i < len; i++) {
		dst[i] = spi_xfer(0xFF);
	}
	while (SPI_BSY(SPIF));
}

void flash25q64_init(void) {
	RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

	FLASH_SCK_INIT();
	FLASH_MISO_INIT();
	FLASH_MOSI_INIT();
	FLASH_NSS_INIT();

	SPIF->CR1 = 0;
	SPIF->CR2 = 0;

	// PCLK2 = 16 MHz (no PLL). /2 => 8 MHz SCK. CPOL=0, CPHA=0.
	SPIF->CR1 =
		SPI_CR1_MSTR |
		SPI_CR1_SSI  |
		SPI_CR1_SSM  |
		SPI_CR1_SPE;
	SPIF->CR1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA);

	// Release from power-down, just in case.
	cs_low();
	(void)spi_xfer(CMD_RELEASE_PD);
	(void)spi_xfer(0xFF);
	(void)spi_xfer(0xFF);
	(void)spi_xfer(0xFF);
	cs_high();
}

static void write_enable(void) {
	cs_low();
	(void)spi_xfer(CMD_WRITE_ENABLE);
	cs_high();
}

static void write_disable(void) {
	cs_low();
	(void)spi_xfer(CMD_WRITE_DISABLE);
	cs_high();
}

uint8_t flash25q64_read_status(void) {
	cs_low();
	(void)spi_xfer(CMD_READ_STATUS_REG1);
	uint8_t s = spi_xfer(0xFF);
	cs_high();
	return s;
}

void flash25q64_wait_busy(void) {
	while (flash25q64_read_status() & 0x01) {
		__NOP();
	}
}

uint32_t flash25q64_read_id(void) {
	cs_low();
	(void)spi_xfer(CMD_JEDEC_ID);
	uint8_t mfg = spi_xfer(0xFF);
	uint8_t mem = spi_xfer(0xFF);
	uint8_t cap = spi_xfer(0xFF);
	cs_high();
	return ((uint32_t)mfg << 16) | ((uint32_t)mem << 8) | cap;
}

void flash25q64_read(uint32_t addr, uint8_t* dst, uint32_t len) {
	cs_low();
	(void)spi_xfer(CMD_READ_DATA);
	(void)spi_xfer((addr >> 16) & 0xFF);
	(void)spi_xfer((addr >> 8) & 0xFF);
	(void)spi_xfer(addr & 0xFF);
	spi_read(dst, len);
	cs_high();
}

void flash25q64_sector_erase(uint32_t addr) {
	write_enable();
	cs_low();
	(void)spi_xfer(CMD_SECTOR_ERASE);
	(void)spi_xfer((addr >> 16) & 0xFF);
	(void)spi_xfer((addr >> 8) & 0xFF);
	(void)spi_xfer(addr & 0xFF);
	cs_high();
	flash25q64_wait_busy();
	write_disable();
}

void flash25q64_chip_erase(void) {
	write_enable();
	cs_low();
	(void)spi_xfer(CMD_CHIP_ERASE);
	cs_high();
	flash25q64_wait_busy();
	write_disable();
}

void flash25q64_program(uint32_t addr, const uint8_t* src, uint32_t len) {
	while (len > 0) {
		uint32_t page_remain = FLASH25Q64_PAGE_SIZE - (addr % FLASH25Q64_PAGE_SIZE);
		uint32_t chunk = len < page_remain ? len : page_remain;

		write_enable();
		cs_low();
		(void)spi_xfer(CMD_PAGE_PROGRAM);
		(void)spi_xfer((addr >> 16) & 0xFF);
		(void)spi_xfer((addr >> 8) & 0xFF);
		(void)spi_xfer(addr & 0xFF);
		spi_write(src, chunk);
		cs_high();
		flash25q64_wait_busy();
		write_disable();

		addr += chunk;
		src += chunk;
		len -= chunk;
	}
}

void flash25q64_erase_and_write(uint32_t addr, const uint8_t* src, uint32_t len) {
	uint32_t end = addr + len;
	for (uint32_t a = addr; a < end; a += FLASH25Q64_SECTOR_SIZE) {
		flash25q64_sector_erase(a);
	}
	flash25q64_program(addr, src, len);
}
