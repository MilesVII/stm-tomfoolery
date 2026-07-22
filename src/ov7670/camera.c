#include "stm32f411xe.h"
#include "../hal_at_home.h"
#include "registers.h"
#include <math.h>
#include <stdarg.h>
#include <string.h>


#define I2C_DEVICE 0x21

#define I2C I2C1

DECLARE_I2C(SCL, B, 6, 4)
DECLARE_I2C(SDA, B, 7, 4)
DECLARE_GPIO_MOUT(RST, B, 2)
DECLARE_GPIO_MOUT(PWDN, B, 1)

// pixel bus
DECLARE_GPIO_MIN(D0, A, 9)
DECLARE_GPIO_MIN(D1, A, 10)
DECLARE_GPIO_MIN(D2, A, 11)
DECLARE_GPIO_MIN(D3, A, 12)
DECLARE_GPIO_MIN(D4, A, 15)
DECLARE_GPIO_MIN(D5, A, 3)
DECLARE_GPIO_MIN(D6, B, 4)
DECLARE_GPIO_MIN(D7, B, 5)

DECLARE_GPIO_MIN(PCLK, A, 2)
DECLARE_GPIO_MIN(HS, A, 3)
DECLARE_GPIO_MIN(VS, A, 1)

DECLARE_CLOCK(XCLK, B, 8)

static void set(uint8_t reg, uint8_t v) {
	I2C_START(I2C);
	I2C_ADDRESS_W(I2C, I2C_DEVICE);
	I2C_SEND(I2C, reg);
	I2C_SEND(I2C, v);
	I2C_STOP(I2C);
}

static uint8_t get(uint8_t reg) {
	I2C_START(I2C);
	I2C_ADDRESS_W(I2C, I2C_DEVICE);
	I2C_SEND(I2C, reg);
	I2C_STOP(I2C);

	I2C_START(I2C);
	I2C_ADDRESS_R(I2C, I2C_DEVICE);
	I2C_ACK_OUT(I2C);
	I2C_STOP(I2C);
	uint8_t data;
	I2C_READ(I2C, data);
	return data;
}

void camera_init() {
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
	RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

	SCL_INIT();
	SDA_INIT();
	RST_INIT(); RST_HIGH();
	PWDN_INIT(); PWDN_LOW();

	D0_INIT();
	D1_INIT();
	D2_INIT();
	D3_INIT();
	D4_INIT();
	D5_INIT();
	D6_INIT();
	D7_INIT();

	PCLK_INIT();
	HS_INIT();
	VS_INIT();

	XCLK_INIT(8000000); // 8 MHz

	I2C->CR1 = I2C_CR1_SWRST;
	I2C->CR1 = 0;

	I2C->CR2 |= 0x10;
	I2C->CCR |= 0x50;
	I2C->TRISE |= (17 << 0);
	I2C->CR1 |= I2C_CR1_PE;

	RST_LOW();
	delay_ms(100);
	RST_HIGH();
	delay_ms(100);

	// https://github.com/Ursadon/ov7670-stm32/blob/master/drivers/ov7670/ov7670.c

	int hstart = 456, hstop = 24, vstart = 14, vstop = 494;
	uint8_t v;
	set(REG_COM7, COM7_RESET);
	delay_ms(10);
	set(REG_CLKRC, 0x01);
	set(REG_COM7, COM7_FMT_QVGA | COM7_YUV);
	set(REG_COM10, COM10_PCLK_HB);
	delay_ms(10);

	set(REG_HSTART, (hstart >> 3) & 0xff);
	set(REG_HSTOP, (hstop >> 3) & 0xff);
	v = get(REG_HREF);
	v = (v & 0xc0) | ((hstop & 0x7) << 3) | (hstart & 0x7);
	set(REG_HREF, v);

	set(REG_VSTART, (vstart >> 2) & 0xff);
	set(REG_VSTOP, (vstop >> 2) & 0xff);
	v = get(REG_VREF);
	v = (v & 0xf0) | ((vstop & 0x3) << 2) | (vstart & 0x3);
	set(REG_VREF, v);

	delay_ms(300);
}


DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

void ledOff() {
	LED_HIGH();
}
void ledOn() {
	LED_LOW();
}


void camera_frame(uint8_t* fb) {
	delay_ms(400);

	size_t tp;
	uint8_t chromaSkip = 0;
	uint16_t col = 0;
	uint16_t row = 0;
	while (VS_READ()); // wait for vsync to drop
	do {
		// while (!HS_READ()); // wait for href to rise
		while (!PCLK_READ()); // wait for PCLK to rise
		if (!chromaSkip) {
			tp = col * 240 + row;
			fb[tp] =
				(!!D0_READ()) << 0 |
				(!!D1_READ()) << 1 |
				(!!D2_READ()) << 2 |
				(!!D3_READ()) << 3 |
				(!!D4_READ()) << 4 |
				(!!D5_READ()) << 5 |
				(!!D6_READ()) << 6 |
				(!!D7_READ()) << 7;
			++col;
		}
		chromaSkip = !chromaSkip;

		if (col == 320) {
			// chromaSkip = 0;
			col = 0;
			++row;
			// return; //dbg
		}
		if (row == 240) return;
		while (PCLK_READ()); // wait for PCLK to fall
	} while(!VS_READ());
}
