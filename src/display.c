#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "display.h"

#define DISPLAY_W  64
#define DISPLAY_H 128

/*
320x240
A5 SCK
A7 MOSI

B0 D/C
B1 RESET
B10 CS
*/


#define SCK_PORT    GPIOA
#define SCK_PIN     5

#define MOSI_PORT   GPIOA
#define MOSI_PIN    7

#define NSS_PORT    GPIOB
#define NSS_PIN     10
#define NSS_HIGH()  PIN_SET(NSS_PORT, GPIO_PIN(NSS_PIN))
#define NSS_LOW()   PIN_CLR(NSS_PORT, GPIO_PIN(NSS_PIN))

#define RST_PORT    GPIOB
#define RST_PIN     1
#define RST_HIGH()  PIN_SET(RST_PORT, GPIO_PIN(RST_PIN))
#define RST_LOW()   PIN_CLR(RST_PORT, GPIO_PIN(RST_PIN))

#define DC_PORT     GPIOB
#define DC_PIN      0
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
	RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

	SPI_PIN_INIT(SCK);
	SPI_PIN_INIT(MOSI);

	OUT_PIN_INIT(NSS);
	OUT_PIN_INIT(DC);
	OUT_PIN_INIT(RST);

	// clear
	SPI1->CR1 = 0;
	SPI1->CR2 = 0;

	SPI1->CR1 =
		SPI_CR1_MSTR |
		SPI_CR1_SSI  |
		SPI_CR1_SSM  |
		SPI_CR1_BR_2 | SPI_CR1_BR_1 |
		SPI_CR1_SPE;
	SPI1->CR1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA);

	SPI1->CR2 = (7 << 8);
}

// Simple blocking transmit (1 byte) + receive (full duplex)
uint8_t SPI1_Transfer(uint8_t data, uint8_t read) {
	while (!SPI_TXE_READY(SPI1));

	// Send data
	SPI1->DR = data;

	// Wait until RX buffer not empty (also ensures transfer finished)
	while (!SPI_RXNE_READY(SPI1));

	// Read received byte
	if (read)
		return (uint8_t) SPI1->DR;
	else
		return (uint8_t) 0;
}

// Blocking transmit array (most common use-case)
void SPI1_Write(uint8_t *data, uint32_t len)
{
	NSS_LOW();

	for (uint32_t i = 0; i < len; i++) {
		SPI1_Transfer(data[i], 0);
	}

	// Wait until not busy (important!)
	while (SPI_BSY(SPI1));

	NSS_HIGH();
}

void reg(uint8_t command) {
	DC_LOW();
	SPI1_Write(&command, 1);
}
void byte(uint8_t byte) {
	DC_HIGH();
	SPI1_Write(&byte, 1);
}

static void display_reset(void) {
	// OLED_RST_1;
	// delay_ms(100);
	// OLED_RST_0;
	// delay_ms(100);
	// OLED_RST_1;
	// delay_ms(100);
}

static void display_regInit(void) {
}

void display_init() {
	//Hardware reset
	display_reset();

	//Set the initialization register
	display_regInit();
	delay_ms(200);

	//Turn on the OLED display
	display_writeReg(0xaf);
}

