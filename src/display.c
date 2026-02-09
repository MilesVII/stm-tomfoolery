#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "display.h"
#include "font.h"
#include <math.h>

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
#define NSS_PIN     6
#define NSS_HIGH()  PIN_SET(NSS_PORT, GPIO_PIN(NSS_PIN))
#define NSS_LOW()   PIN_CLR(NSS_PORT, GPIO_PIN(NSS_PIN))

#define RST_PORT    GPIOA
#define RST_PIN     9
#define RST_HIGH()  PIN_SET(RST_PORT, GPIO_PIN(RST_PIN))
#define RST_LOW()   PIN_CLR(RST_PORT, GPIO_PIN(RST_PIN))

#define DC_PORT     GPIOA
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
static uint8_t SPI1_Transfer(uint8_t data, uint8_t read) {
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
static void SPI1_Write(uint8_t *data, uint32_t len) {
	NSS_LOW();

	for (uint32_t i = 0; i < len; i++) {
		SPI1_Transfer(data[i], 0);
	}

	// Wait until not busy (important!)
	while (SPI_BSY(SPI1));

	NSS_HIGH();
}

static void reg(uint8_t command) {
	DC_LOW();
	SPI1_Write(&command, 1);
}
static void data(uint8_t byte) {
	DC_HIGH();
	SPI1_Write(&byte, 1);
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
	reg(0xAE); //--turn off oled panel

	reg(0x02); //---set low column address
	reg(0x10); //---set high column address

	reg(0x40); //--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)
	reg(0x81); //--set contrast control register
	reg(0xA0); //--Set SEG/Column Mapping a0/a1
	reg(0xC0); //Set COM/Row Scan Direction
	reg(0xA6); //--set normal display a6/a7
	reg(0xA8); //--set multiplex ratio(1 to 64)
	reg(0x3F); //--1/64 duty
	reg(0xD3); //-set display offset    Shift Mapping RAM Counter (0x00~0x3F)
	reg(0x00); //-not offset
	reg(0xd5); //--set display clock divide ratio/oscillator frequency
	reg(0x80); //--set divide ratio, Set Clock as 100 Frames/Sec
	reg(0xD9); //--set pre-charge period
	reg(0xF1); //Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
	reg(0xDA); //--set com pins hardware configuration
	reg(0x12);
	reg(0xDB); //--set vcomh
	reg(0x40); //Set VCOM Deselect Level
	reg(0x20); //-Set Page Addressing Mode (0x00/0x01/0x02)
	reg(0x02); //
	reg(0xA4); // Disable Entire Display On (0xa4/0xa5)
	reg(0xA6); // Disable Inverse Display On (0xa6/a7)
}

void display_init() {
	//Hardware reset
	display_reset();

	//Set the initialization register
	display_regInit();
	delay_ms(200);

	//Turn on the OLED display
	reg(0xaf);
}

void display_clear(void) {
	// UWORD Width, Height;
	uint16_t i, j;
	// Width = (OLED_1IN3_WIDTH % 8 == 0)? (OLED_1IN3_WIDTH / 8 ): (OLED_1IN3_WIDTH / 8 + 1);
	// Height = OLED_1IN3_HEIGHT;
	for (i = 0; i < 8; ++i) {
		/* set page address */
		reg(0xB0 + i);
		/* set low column address */
		reg(0x02);
		/* set high column address */
		reg(0x10);
		for(j = 0; j < 128; j++) {
			/* write data */
			data(0x00);
		}
	}
}

void display_update(uint8_t targetPage) {
	uint16_t page, row, temp;

	for (page = 0; page < 8; ++page) {
		/* set page address */
		reg(0xB0 + page);
		/* set low column address */
		reg(0x02);
		/* set high column address */
		reg(0x10);

		/* write data */
		for(row = 0; row < 128; ++row) {
			if (page == targetPage && (row / 8 == targetPage)) {
				data(0xFF);
			} else {
				data(0x00);
			}
			// temp = 0xF0; //Image[(7-page) + column*8];
		}
	}
}

#define PAGE_CAP (DISPLAY_W / 8)
#define CHAR_CAP 10 // PAGE_CAP * 2;
static uint8_t drawChar(uint16_t page, uint16_t row, uint32_t status) {
	int exp = CHAR_CAP / 2 - 1 - page;
	if (exp < 0) return 0;

	uint32_t rightTrim = pow(100, exp);
	uint16_t hundreds = (status / rightTrim) % 100;
	uint8_t high = hundreds / 10;
	uint8_t low =  hundreds % 10;
	return (CHARACTERS[low][row] << 4) | (CHARACTERS[high][row]);
}
void display_update_48_32(uint8_t* frame, uint32_t status) {
	uint16_t page, row, x, y;

	/*
	[row2]
	[row1]
	[row0]
	x [p0] [p1] [p2] etc
	*/
	for (page = 0; page < PAGE_CAP; ++page) {
		/* set page address */
		reg(0xB0 + page);
		/* set low column address */
		reg(0x02);
		/* set high column address */
		reg(0x10);

		/* write data */
		for(row = 0; row < DISPLAY_H; ++row) {
			if (row < 48 || row >= 80 || page == 0 || page == 7) {
				if (row < 7) {
					data(drawChar(page, row, status));
				} else {
					data(0x00);
				}
			} else {
				y = row - 48;
				x = page - 1;
				// actually just it's the same as reading bytes in direct order
				data(frame[x * 32 + y]);
			}
		}
	}
}
