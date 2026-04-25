#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "display.h"
#include <math.h>
#include <stdarg.h>

/*
320x240
SPI2:
B14(hanging) SDO/MISO
B10 SCK
B15 SDI/MOSI

GPIO:
B8 DC
B7 RESET
B9 CS
*/
#define SPI SPI2

#define SCK_PORT    GPIOB
#define SCK_PIN     10

#define MOSI_PORT   GPIOB
#define MOSI_PIN    15

#define NSS_PORT    GPIOB
#define NSS_PIN     9
#define NSS_HIGH()  PIN_SET(NSS_PORT, GPIO_PIN(NSS_PIN))
#define NSS_LOW()   PIN_CLR(NSS_PORT, GPIO_PIN(NSS_PIN))

#define RST_PORT    GPIOB
#define RST_PIN     7
#define RST_HIGH()  PIN_SET(RST_PORT, GPIO_PIN(RST_PIN))
#define RST_LOW()   PIN_CLR(RST_PORT, GPIO_PIN(RST_PIN))

#define DC_PORT     GPIOB
#define DC_PIN      8
#define DC_HIGH()   PIN_SET(DC_PORT, GPIO_PIN(DC_PIN))
#define DC_LOW()    PIN_CLR(DC_PORT, GPIO_PIN(DC_PIN))

#define SPI_PIN_INIT(pin) \
	MODER(pin##_PORT, pin##_PIN, 2); \
	AFR(pin##_PORT, pin##_PIN, 5); \
	OSPEEDR(pin##_PORT, pin##_PIN, 3);
#define OUT_PIN_INIT(pin) \
	MODER(pin##_PORT, pin##_PIN, 1); \
	pin##_HIGH();

void display_initSPI() {
	RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
	// RCC->APB2ENR |= RCC_APB2ENR_SPI4EN;

	SPI_PIN_INIT(SCK);
	SPI_PIN_INIT(MOSI);
	OUT_PIN_INIT(NSS);
	OUT_PIN_INIT(DC);
	OUT_PIN_INIT(RST);

	// clear
	SPI->CR1 = 0;
	SPI->CR2 = 0;

	SPI->CR1 =
		SPI_CR1_MSTR |
		SPI_CR1_SSI  |
		SPI_CR1_SSM  |
		0; //BR /2
	SPI->CR1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA);

	// SPI->CR2 = (7 << 8);
	
	SPI->CR1 |= SPI_CR1_SPE;
}

// Simple blocking transmit (1 byte) + receive (full duplex)
static uint8_t SPI_Transfer(uint8_t data) {
	while (!SPI_TXE_READY(SPI));

	// Send data
	SPI->DR = data;

	// Wait until RX buffer not empty (also ensures transfer finished)
	while (!SPI_RXNE_READY(SPI));

	// Read received byte
	return (uint8_t) SPI->DR;
}

// Blocking transmit array (most common use-case)
static void SPI_Write(uint8_t *data, uint32_t len) {
	NSS_LOW();

	for (uint32_t i = 0; i < len; i++) {
		SPI_Transfer(data[i]);
	}

	// Wait until not busy (important!)
	while (SPI_BSY(SPI));

	NSS_HIGH();
}

static void SPI_WriteFill(uint8_t data, uint32_t len) {
	NSS_LOW();

	for (uint32_t i = 0; i < len; i++) {
		SPI_Transfer(data);
	}

	// Wait until not busy (important!)
	while (SPI_BSY(SPI));

	NSS_HIGH();
}

static void reg(uint8_t command) {
	DC_LOW();
	SPI_Write(&command, 1);
}
static void stream(uint8_t* byte, uint32_t count) {
	DC_HIGH();
	SPI_Write(byte, count);
}
static void fill(uint8_t byte, uint32_t count) {
	DC_HIGH();
	SPI_WriteFill(byte, count);
}
static void data(uint8_t byte) {
	DC_HIGH();
	SPI_Write(&byte, 1);
}
static void command(int count, ...) {
	va_list args;
	va_start(args, count);

	for (int i = 0; i < count; i++) {
		uint8_t byte = (uint8_t)va_arg(args, int);
		if (i == 0) reg(byte);
		else data(byte);
	}

	va_end(args); // Clean up
}

static void display_reset(void) {
	RST_HIGH();
	delay_ms(100);
	RST_LOW();
	delay_ms(100);
	RST_HIGH();
	delay_ms(100);
}

static void display_regInit(void) {
	reg(0x01); // SWRESET;
	delay_ms(150);

	reg(0x28); // off

	command(6, 0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02); // power control A
	command(4, 0xCF, 0x00, 0xC1, 0x30); // power control B
	command(4, 0xE8, 0x85, 0x00, 0x78); // driver timing control
	command(5, 0xED, 0x64, 0x03, 0x12, 0x81); // power on sequence control
	command(2, 0xC5, 0x20); // vcom control 1
	command(2, 0xC7, 0x55); // vcom control 2
	command(16, 0xE0, 0x0F, 0x29, 0x24, 0x0C, 0x0E, 0x09, 0x4E, 0x78, 0x3C, 0x09, 0x13, 0x05, 0x17, 0x11, 0x00);
	command(16, 0xE1, 0x00, 0x16, 0x1B, 0x04, 0x11, 0x07, 0x31, 0x33, 0x42, 0x05, 0x0C, 0x0A, 0x28, 0x2F, 0x0F);

	command(2, 0x3A, 0x55);       // RGB565
	command(2, 0x36, 0b10010100); // memory access control

	reg(0x11); // sleep out
	delay_ms(120);
	reg(0x29); // on
}

void display_init() {
	display_reset();
	display_regInit();
}

#define HI(v) ((v) >> 8)
#define LO(v) ((v) & 0xFF)
#define SEND16(v) \
	data(HI(v)); \
	data(LO(v));
uint32_t display_setWindow(
	uint16_t x0, uint16_t y0,
	uint16_t w, uint16_t h
) {
	reg(0x2A); // column
	SEND16(x0);
	SEND16(x0 + w - 1);
	reg(0x2B); // row
	SEND16(y0);
	SEND16(y0 + h - 1);
	return w * h;
}

void display_sendBytes(uint16_t* pixels, uint32_t pixelCount) {
	reg(0x2C);
	stream((uint8_t*)pixels, pixelCount * 2);
}

void display_clear(uint8_t halfColor) {
	uint32_t count = display_setWindow(0, 0, 240, 320);
	reg(0x2C);
	fill(halfColor, count * 2);
}
