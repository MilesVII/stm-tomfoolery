#include "stm32f411xe.h"
#include "../hal_at_home.h"
#include "thermal.h"

#define SPI SPI3
DECLARE_SPI(SCK, B, 3, 6);
DECLARE_SPI(MISO, B, 4, 6);
DECLARE_GPIO_MOUT(NSS, B, 5);

void thermal_init() {
	RCC->APB1ENR |= RCC_APB1ENR_SPI3EN;
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

	SCK_INIT();
	MISO_INIT();
	NSS_INIT();

	// clear
	SPI->CR1 = 0;
	SPI->CR2 = 0;

	SPI->CR1 =
		SPI_CR1_MSTR |
		SPI_CR1_SSI  |
		SPI_CR1_SSM  |
		SPI_CR1_BR_2 | SPI_CR1_BR_1;
	SPI->CR1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA);

	SPI->CR1 |= SPI_CR1_SPE;
}

static uint8_t transfer(uint8_t data) {
	while (!SPI_TXE_READY(SPI));
	SPI->DR = data;
	while (!SPI_RXNE_READY(SPI));
	(void)SPI->SR;
	return (uint8_t) SPI->DR;
}

float thermal_poll() {
	uint16_t value;

	NSS_LOW();
	
	value  = transfer(0x00) << 8;
	value |= transfer(0x00);

	NSS_HIGH();
	
	if (value & 0x0004) {
		// thermocouple disconnected
		return THERMAL_DISCONNECTED;
	}

	return ((value >> 3) & 0x0FFF) * 0.25f;
}
