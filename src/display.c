#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "display.h"
#include "font.h"
#include <math.h>
#include <stdarg.h>

/*
240x320
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

DECLARE_SPI(SCK , B, 10, 5);
DECLARE_SPI(MOSI, B, 15, 5);
DECLARE_GPIO_MOUT(NSS, B, 9);
DECLARE_GPIO_MOUT(RST, B, 2);
DECLARE_GPIO_MOUT(DC , B, 8);

void display_initSPI() {
	RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

	SCK_INIT();
	MOSI_INIT();
	NSS_INIT();
	DC_INIT();
	RST_INIT();

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

void display_init() {
	RST_HIGH();
	delay_ms(100);
	RST_LOW();
	delay_ms(100);
	RST_HIGH();
	delay_ms(100);

	reg(0x01); // SWRESET;
	delay_ms(150);

	reg(0x28); // off

	reg(0x21); // color inversion
	// command(2, 0xC0, 0x23); // power control A
	// command(2, 0xC1, ____); // power control B
	// command(3, 0xC5, 0x3E, 0x28); // vcom control 1
	// command(2, 0xC7, 0x86); // vcom control 2

	// command(4, 0xE8, 0x85, 0x00, 0x78); // driver timing control A
	// command(5, 0xED, 0x64, 0x03, 0x12, 0x81); // power on sequence control
	// command(16, 0xE0, 0x0F, 0x29, 0x24, 0x0C, 0x0E, 0x09, 0x4E, 0x78, 0x3C, 0x09, 0x13, 0x05, 0x17, 0x11, 0x00);
	// command(16, 0xE1, 0x00, 0x16, 0x1B, 0x04, 0x11, 0x07, 0x31, 0x33, 0x42, 0x05, 0x0C, 0x0A, 0x28, 0x2F, 0x0F);

	command(2, 0x3A, 0x55);       // RGB565
	command(2, 0x36, 0b00001000); // memory access control

	reg(0x11); // sleep out
	delay_ms(120);
	reg(0x29); // on
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

void display_clear(uint8_t halfColor, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
	uint32_t count = display_setWindow(x, y, w, h);
	reg(0x2C);
	fill(halfColor, count * 2);
}

void display_digit(uint16_t* gfx, uint8_t v, uint16_t atX, uint16_t atY, uint16_t backColor, uint16_t foreColor) {
	uint16_t* cursor = gfx;

	uint32_t pc = display_setWindow(atX, atY, DIGIT_W, DIGIT_H);
	for (int y = DIGIT_H - 1; y >= 0; --y) {
		uint8_t row = CHARACTERS[v][y/2];
		for (int x = 0; x < DIGIT_W; ++x) {
			uint8_t bright = row & (1 << (3 - x/2));
			*cursor = bright ? foreColor : backColor;
			++cursor;
		}
	}
	display_sendBytes(gfx, pc);
}

void display_number(uint16_t* gfx, uint16_t v, uint16_t atX, uint16_t atY) {
	display_clear(0x11, 0, atY, 240, DIGIT_H);
	while(1) {
		atX -= DIGIT_W;
		uint8_t digit = v % 10;
		display_digit(gfx, digit, atX, atY, 0x0000, 0xFFFF);
		v /= 10;
		if (v <= 0) return;
	}
}

static int patternA(int x){
	return ((x*x)&x);
}
static int patternB(int x){
	return (x>>7)&x;
}
uint8_t buf[480];
void display_pattern() {
	uint8_t c0 = 0b11011010;
	uint8_t c1 = 0b00000000;
	uint32_t count = display_setWindow(0, 0, 240, 320);
	reg(0x2C);
	for (int y = 0; y < 320; ++y) {
		for (int x = 0; x < 240; ++x) {
			int v = (x + 120) ^ y;
			uint8_t c = patternA(v) > patternB(v) ? 0 : 1;
			buf[x * 2] = c0 * c;
			buf[x * 2 + 1] = c1 * c;
		}
		stream(buf, 480);
	}
	// fill(halfColor, count * 2);
}
