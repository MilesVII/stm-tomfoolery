#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "display.h"
#include <math.h>

#define DISPLAY_W 240
#define DISPLAY_H 320

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

	SPI_PIN_INIT(SCK);
	SPI_PIN_INIT(MOSI);
GPIOB->AFR;
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
		SPI_CR1_BR_2 | SPI_CR1_BR_1;
	SPI->CR1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA);

	SPI->CR2 = (7 << 8);
	
	SPI->CR1 |= SPI_CR1_SPE;
}

// Simple blocking transmit (1 byte) + receive (full duplex)
static uint8_t SPI_Transfer(uint8_t data, uint8_t read) {
	while (!SPI_TXE_READY(SPI));

	// Send data
	SPI->DR = data;

	// Wait until RX buffer not empty (also ensures transfer finished)
	while (!SPI_RXNE_READY(SPI));

	// Read received byte
	if (read)
		return (uint8_t) SPI->DR;
	else
		return (uint8_t) 0;
}

// Blocking transmit array (most common use-case)
static void SPI_Write(uint8_t *data, uint32_t len) {
	NSS_LOW();

	for (uint32_t i = 0; i < len; i++) {
		SPI_Transfer(data[i], 0);
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
static void data(uint8_t byte) {
	DC_HIGH();
	SPI_Write(&byte, 1);
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
	
	reg(0x3A); // pixel format
	data(0x55); // rgb565

	reg(0x36); // x-right y-up landscape please thanks
	data(0b10010100);

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
