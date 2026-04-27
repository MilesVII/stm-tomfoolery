#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "touch.h"
#include <float.h>
#include <math.h>

#define SPI SPI3

#define SCK_PORT    GPIOB
#define SCK_PIN     3

#define MOSI_PORT   GPIOB
#define MOSI_PIN    5

#define MISO_PORT   GPIOB
#define MISO_PIN    4

#define NSS_PORT    GPIOA
#define NSS_PIN     15
#define NSS_HIGH()  PIN_SET(NSS_PORT, GPIO_PIN(NSS_PIN))
#define NSS_LOW()   PIN_CLR(NSS_PORT, GPIO_PIN(NSS_PIN))

#define PIRQ_PORT   GPIOB
#define PIRQ_PIN    6

#define SPI_PIN_INIT(pin) \
	MODER(pin##_PORT, pin##_PIN, 2); \
	AFR(pin##_PORT, pin##_PIN, 6); \
	OSPEEDR(pin##_PORT, pin##_PIN, 3);
#define OUT_PIN_INIT(pin) \
	MODER(pin##_PORT, pin##_PIN, 1); \
	pin##_HIGH();

const uint8_t CTRL_X = 0b11010001;
const uint8_t CTRL_Y = 0b10010000;

const float CAL_LO_X = 1890.0;
const float CAL_HI_X = 120.0;
const float CAL_LO_Y = 1940.0;
const float CAL_HI_Y = 180.0;
float remap(float x, float fromMin, float fromMax, float toMin, float toMax) {
	return (x - fromMin) * (toMax - toMin) / (fromMax - fromMin) + toMin;
}

void touch_calibrate(uint16_t* x, uint16_t* y) {
	float fx = (float)(*x);
	float fy = (float)(*y);
	float ax = remap(fx, CAL_LO_X, CAL_HI_X, 0.0, 240.0);
	float ay = remap(fy, CAL_LO_Y, CAL_HI_Y, 0.0, 320.0);
	if (ax < 0) ax = 0;
	// if (ax > 240) ax = 240;
	if (ay < 0) ay = 0;
	// if (ay > 320) ay = 320;

	*x = (uint16_t)ax;
	*y = (uint16_t)ay;
}

void touch_initSPI() {
	RCC->APB1ENR |= RCC_APB1ENR_SPI3EN;

	SPI_PIN_INIT(SCK);
	SPI_PIN_INIT(MOSI);
	SPI_PIN_INIT(MISO);
	OUT_PIN_INIT(NSS);
	PUPDR(PIRQ_PORT, PIRQ_PIN, 1);

	// clear
	SPI->CR1 = 0;
	SPI->CR2 = 0;

	SPI->CR1 =
		SPI_CR1_MSTR |
		SPI_CR1_SSI  |
		SPI_CR1_SSM  |
		SPI_CR1_BR_1 | SPI_CR1_BR_2;
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
static uint16_t SPI_Sub(uint8_t data) {
	SPI_Transfer(data);
	uint8_t hi = SPI_Transfer(0x00);
	uint8_t lo = SPI_Transfer(0x00);
	return ((hi << 8) | lo) >> 4;
}

void touch_poll(uint16_t* x, uint16_t* y) {
	(void)SPI->DR;
	(void)SPI->SR;

	NSS_LOW();

	*x = SPI_Sub(CTRL_X);
	*y = SPI_Sub(CTRL_Y);

	// Wait until not busy (important!)
	while (SPI_BSY(SPI));

	NSS_HIGH();
}

uint8_t touch_up() {
	return PIRQ_PORT->IDR & (1 << PIRQ_PIN);
}
