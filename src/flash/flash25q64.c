#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "flash25q64.h"

// 25Q64JVSIQ 8 MiB SPI flash driver
// Uses SPI1 (PA4 NSS, PA5 SCK, PA6 MISO, PA7 MOSI)

#define SPIF SPI1

DECLARE_SPI(FLASH_SCK , A, 5, 5);
DECLARE_SPI(FLASH_MISO, A, 6, 5);
DECLARE_SPI(FLASH_MOSI, A, 7, 5);
DECLARE_GPIO_MOUT(FLASH_CS, A, 4);

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

static uint8_t spi_byte(const uint8_t byte) {
	while (!SPI_TXE_READY(SPIF));
	SPIF->DR = byte;
	while (!SPI_RXNE_READY(SPIF));
	uint8_t hehe = (uint8_t)SPIF->DR;
	while (SPI_BSY(SPIF));
	return hehe;
}

static void spi_write(const uint8_t* data, uint32_t len) {
	for (uint32_t i = 0; i < len; ++i) {
		while (!SPI_TXE_READY(SPIF));
		SPIF->DR = data[i];
		while (!SPI_RXNE_READY(SPIF));
		(void)SPIF->DR;
	}
	while (SPI_BSY(SPIF));
}

static void spi_read(uint8_t* dst, uint32_t len) {
	for (uint32_t i = 0; i < len; ++i) {
		while (!SPI_TXE_READY(SPIF));
		SPIF->DR = 0xFF;
		while (!SPI_RXNE_READY(SPIF));
		dst[i] = (uint8_t)SPIF->DR;
	}
	while (SPI_BSY(SPIF));
}

void flash25q64_init(void) {
	RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

	FLASH_SCK_INIT();
	FLASH_MISO_INIT();
	FLASH_MOSI_INIT();
	FLASH_CS_INIT(); FLASH_CS_HIGH();

	SPIF->CR1 = 0;
	SPIF->CR2 = 0;

	SPIF->CR1 =
		SPI_CR1_MSTR |
		SPI_CR1_SSI  |
		SPI_CR1_SSM  |
		(0U << 3);
	SPIF->CR1 |= SPI_CR1_SPE;

	// Release from power-down, just in case.
	FLASH_CS_LOW();
	spi_byte(CMD_RELEASE_PD);
	spi_byte(0xFF);
	spi_byte(0xFF);
	spi_byte(0xFF);
	FLASH_CS_HIGH();
}

static void write_enable(void) {
	FLASH_CS_LOW();
	spi_byte(CMD_WRITE_ENABLE);
	FLASH_CS_HIGH();
	while (flash25q64_read_status() & 0x02 == 0);
}

static void write_disable(void) {
	FLASH_CS_LOW();
	spi_byte(CMD_WRITE_DISABLE);
	FLASH_CS_HIGH();
}

uint8_t flash25q64_read_status(void) {
	FLASH_CS_LOW();
	spi_byte(CMD_READ_STATUS_REG1);
	uint8_t s = spi_byte(0xFF);
	FLASH_CS_HIGH();
	return s;
}

void flash25q64_wait_busy(void) {
	while (flash25q64_read_status() & 0x01) {
		__NOP();
	}
}

uint32_t flash25q64_read_id(void) {
	FLASH_CS_LOW();
	spi_byte(CMD_JEDEC_ID);
	uint8_t mfg = spi_byte(0xFF);
	uint8_t mem = spi_byte(0xFF);
	uint8_t cap = spi_byte(0xFF);
	FLASH_CS_HIGH();
	return ((uint32_t)mfg << 16) | ((uint32_t)mem << 8) | cap;
}

void flash25q64_read(uint32_t addr, uint8_t* dst, uint32_t len) {
	FLASH_CS_LOW();
	spi_byte(CMD_READ_DATA);
	spi_byte((addr >> 16) & 0xFF);
	spi_byte((addr >> 8) & 0xFF);
	spi_byte(addr & 0xFF);
	spi_read(dst, len);
	FLASH_CS_HIGH();
}

void flash25q64_sector_erase(uint32_t addr) {
	write_enable();
	FLASH_CS_LOW();
	spi_byte(CMD_SECTOR_ERASE);
	spi_byte((addr >> 16) & 0xFF);
	spi_byte((addr >> 8) & 0xFF);
	spi_byte(addr & 0xFF);
	FLASH_CS_HIGH();
	flash25q64_wait_busy();
	write_disable();
}

void flash25q64_chip_erase(void) {
	write_enable();
	FLASH_CS_LOW();
	spi_byte(CMD_CHIP_ERASE);
	FLASH_CS_HIGH();
	flash25q64_wait_busy();
	write_disable();
}

void flash25q64_program(uint32_t addr, const uint8_t* src, uint32_t len) {
	while (len > 0) {
		uint32_t page_remain = FLASH25Q64_PAGE_SIZE - (addr % FLASH25Q64_PAGE_SIZE);
		uint32_t chunk = len < page_remain ? len : page_remain;

		write_enable();
		FLASH_CS_LOW();
		spi_byte(CMD_PAGE_PROGRAM);
		spi_byte((addr >> 16) & 0xFF);
		spi_byte((addr >> 8) & 0xFF);
		spi_byte(addr & 0xFF);
		spi_write(src, chunk);
		FLASH_CS_HIGH();
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
