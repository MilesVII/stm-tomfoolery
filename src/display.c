#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "display.h"

#define DISPLAY_W  64
#define DISPLAY_H 128

void display_initSPI() {
	RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

	/*
	A5 CLK
	A7 DIN

	B6 CS
	A8 D/C
	A1 RES
	*/

	// AF mode
	MODER(GPIOA, 5, 2)
	MODER(GPIOA, 7, 2)

	MODER(GPIOB, 6, 1)
	MODER(GPIOA, 8, 1)
	MODER(GPIOA, 1, 1)

	AFR(GPIOA, 5, 5)
	AFR(GPIOA, 7, 5)

	// OSPEEDR(GPIOA, 5, 3)
	// OSPEEDR(GPIOA, 7, 3)

	SPI1->CR1 =
		SPI_CR1_MSTR |
		SPI_CR1_SSI  |
		SPI_CR1_SSM  |
		SPI_CR1_BR_2 | SPI_CR1_BR_1 |
		SPI_CR1_SPE;
	SPI1->CR1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA);

	SPI1->CR2 = (7 << 8);

	OLED_CS_1;
	OLED_DC_1;
	OLED_RST_1;

	OLED_RST_0;
	delay_ms(100);
	OLED_RST_1;
	delay_ms(100);
}

static void display_reset(void) {
	OLED_RST_1;
	delay_ms(100);
	OLED_RST_0;
	delay_ms(100);
	OLED_RST_1;
	delay_ms(100);
}

static void display_spi4wByte(uint8_t value) {
	SPI1->CR2 |= (1) << 12;
	while (!(SPI1->SR & SPI_SR_TXE));
	
	*((__IO uint8_t *)(&SPI1->DR)) = value;
	while (SPI1->SR & SPI_SR_BSY);
	// while (!(SPI1->SR & SPI_SR_RXNE));
	// return *((__IO uint8_t *)(&SPI1->DR));
}

static void display_writeReg(uint8_t Reg) {
	OLED_DC_0;
	OLED_CS_0;
	display_spi4wByte(Reg);
	OLED_CS_1;
}

static void display_writeData(uint8_t Data) {
	OLED_DC_1;
	OLED_CS_0;
	display_spi4wByte(Data);
	OLED_CS_1;
}

static void display_regInit(void) {
	display_writeReg(0xAE);//--turn off oled panel

	display_writeReg(0x02);//---set low column address
	display_writeReg(0x10);//---set high column address

	display_writeReg(0x40);//--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)
	display_writeReg(0x81);//--set contrast control register
	display_writeReg(0xA0);//--Set SEG/Column Mapping a0/a1
	display_writeReg(0xC0);//Set COM/Row Scan Direction
	display_writeReg(0xA6);//--set normal display a6/a7
	display_writeReg(0xA8);//--set multiplex ratio(1 to 64)
	display_writeReg(0x3F);//--1/64 duty
	display_writeReg(0xD3);//-set display offset    Shift Mapping RAM Counter (0x00~0x3F)
	display_writeReg(0x00);//-not offset
	display_writeReg(0xd5);//--set display clock divide ratio/oscillator frequency
	display_writeReg(0x80);//--set divide ratio, Set Clock as 100 Frames/Sec
	display_writeReg(0xD9);//--set pre-charge period
	display_writeReg(0xF1);//Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
	display_writeReg(0xDA);//--set com pins hardware configuration
	display_writeReg(0x12);
	display_writeReg(0xDB);//--set vcomh
	display_writeReg(0x40);//Set VCOM Deselect Level
	display_writeReg(0x20);//-Set Page Addressing Mode (0x00/0x01/0x02)
	display_writeReg(0x02);//
	display_writeReg(0xA4);// Disable Entire Display On (0xa4/0xa5)
	display_writeReg(0xA6);// Disable Inverse Display On (0xa6/a7)
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

void display_clear() {
	// UWORD Width, Height;
	UWORD i, j;
	// Width = (OLED_1IN3_WIDTH % 8 == 0)? (OLED_1IN3_WIDTH / 8 ): (OLED_1IN3_WIDTH / 8 + 1);
	// Height = OLED_1IN3_HEIGHT;
	for (i=0; i<8; i++) {
		/* set page address */
		display_writeReg(0xB0 + i);
		/* set low column address */
		display_writeReg(0x02);
		/* set high column address */
		display_writeReg(0x10);
		for(j=0; j<128; j++) {
			/* write data */
			display_writeData(0x00);
		}
	}
}

void display_update(/*const UBYTE *Image*/) {
	UWORD page, column, temp;

	for (page=0; page<8; page++) {
		/* set page address */
		display_writeReg(0xB0 + page);
		/* set low column address */
		display_writeReg(0x02);
		/* set high column address */
		display_writeReg(0x10);

		/* write data */
		for(column=0; column<128; column++) {
			// temp = Image[(7-page) + column*8];
			// display_writeData(temp);
			display_writeData(0xF0F0);
		}
	}
}
