#include "stm32f411xe.h"
#include "../hal_at_home.h"
#include "registers.h"
#include <math.h>
#include <stdarg.h>
#include <string.h>


#define I2C_DEVICE 0x21
// #define I2C_DEVICE_R 0x43

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

DECLARE_GPIO_MIN(PCLK, A, 0)
DECLARE_GPIO_MIN(HS, B, 9)
DECLARE_GPIO_MIN(VS, A, 1)
// DECLARE_GPIO_MOUT(XCLK, B, 8)


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

	PCLK_INIT();
	HS_INIT();

	D0_INIT();
	D1_INIT();
	D2_INIT();
	D3_INIT();
	D4_INIT();
	D5_INIT();
	D6_INIT();
	D7_INIT();

	I2C->CR1 = I2C_CR1_SWRST;
	I2C->CR1 = 0;

	I2C->CR2 |= 0x10;
	I2C->CCR |= 0x50;
	I2C->TRISE |= (17 << 0);
	I2C->CR1 |= I2C_CR1_PE;

	RST_LOW();
	delay_ms(100);
	RST_HIGH();

	// https://github.com/Ursadon/ov7670-stm32/blob/master/drivers/ov7670/ov7670.c

	int hstart = 456, hstop = 24, vstart = 14, vstop = 494;
	uint8_t v;
	set(REG_COM7, COM7_RESET);
	set(REG_CLKRC, 0x01);
	set(REG_COM7, COM7_FMT_QVGA | COM7_PBAYER); /* output format: YUCV */

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


	// set(REG_COM5, 0x61);
	// set(REG_COM6, 0x4b);
	// set(0x16, 0x02);
	// set(REG_MVFP, 0x07);
	// set(0x21, 0x02);
	// set(0x22, 0x91);
	// set(0x29, 0x07);
	// set(0x33, 0x0b);
	// set(0x35, 0x0b);
	// set(0x37, 0x1d);
	// set(0x38, 0x71);
	// set(0x39, 0x2a);
	// set(REG_COM12, 0x78);
	// set(0x4d, 0x40);
	// set(0x4e, 0x20);
	// set(REG_GFIX, 0);
	// set(0x6b, 0x4a);
	// set(0x74, 0x10);
	// set(0x8d, 0x4f);
	// set(0x8e, 0);
	// set(0x8f, 0);
	// set(0x90, 0);
	// set(0x91, 0);
	// set(0x96, 0);
	// set(0x9a, 0);
	// set(0xb0, 0x84);
	// set(0xb1, 0x0c);
	// set(0xb2, 0x0e);
	// set(0xb3, 0x82);
	// set(0xb8, 0x0a);
}

void camera_frame() {}

