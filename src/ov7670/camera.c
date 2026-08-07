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
DECLARE_GPIO_MOUT(RST, B, 3)
DECLARE_GPIO_MOUT(PWDN, A, 15)

// pixel bus
DECLARE_GPIO_MIN(D0, A, 0)
DECLARE_GPIO_MIN(D1, A, 1)
DECLARE_GPIO_MIN(D2, A, 2)
DECLARE_GPIO_MIN(D3, A, 3)
DECLARE_GPIO_MIN(D4, A, 4)
DECLARE_GPIO_MIN(D5, A, 5)
DECLARE_GPIO_MIN(D6, A, 6)
DECLARE_GPIO_MIN(D7, A, 7)

DECLARE_GPIO_MIN(PCLK, B, 0)
DECLARE_GPIO_MIN(HS, B, 4)
DECLARE_GPIO_MIN(VS, B, 5)

DECLARE_CLOCK_B8(XCLK)

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
	set(REG_TSLB, 0x00);
	set(REG_CLKRC, 0x3F);
	set(REG_COM7, COM7_FMT_QVGA | COM7_YUV);
	// set(REG_COM15, COM15_RGB565 | COM15_R00FF);

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

	set(REG_COM3, COM3_SCALEEN | COM3_DCWEN);
	set(REG_COM14, COM14_DCWEN | 0x01);
	set(0x73, 0xf1);
	set(0xa2, 0x52);
	set(0x7b, 0x1c);
	set(0x7c, 0x28);
	set(0x7d, 0x3c);
	set(0x7f, 0x69);
	set(REG_COM9, 0x38);
	set(0xa1, 0x0b);
	set(0x74, 0x19);
	set(0x9a, 0x80);
	set(0x43, 0x14);
	set(REG_COM13, 0xc0);
	set(0x70, 0x3A);
	set(0x71, 0x35);
	set(0x72, 0x11);

	/* Gamma curve values */
	set(0x7a, 0x20);
	set(0x7b, 0x10);
	set(0x7c, 0x1e);
	set(0x7d, 0x35);
	set(0x7e, 0x5a);
	set(0x7f, 0x69);
	set(0x80, 0x76);
	set(0x81, 0x80);
	set(0x82, 0x88);
	set(0x83, 0x8f);
	set(0x84, 0x96);
	set(0x85, 0xa3);
	set(0x86, 0xaf);
	set(0x87, 0xc4);
	set(0x88, 0xd7);
	set(0x89, 0xe8);

	/* AGC and AEC parameters.  Note we start by disabling those features,
	 then turn them only after tweaking the values. */
	set(REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT);
	set(REG_GAIN, 0);
	set(REG_AECH, 0);
	set(REG_COM4, 0x40); /* magic reserved bit */
	set(REG_COM9, 0x18); /* 4x gain + magic rsvd bit */
	set(REG_BD50MAX, 0x05);
	set(REG_BD60MAX, 0x07);
	set(REG_AEW, 0x95);
	set(REG_AEB, 0x33);
	set(REG_VPT, 0xe3);
	set(REG_HAECC1, 0x78);
	set(REG_HAECC2, 0x68);
	set(0xa1, 0x03); /* magic */
	set(REG_HAECC3, 0xd8);
	set(REG_HAECC4, 0xd8);
	set(REG_HAECC5, 0xf0);
	set(REG_HAECC6, 0x90);
	set(REG_HAECC7, 0x94);
	set(REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT | COM8_AGC | COM8_AEC);

	/* Almost all of these are magic "reserved" values.  */
	set(REG_COM5, 0x61);
	set(REG_COM6, 0x4b);
	set(0x16, 0x02);
	set(REG_MVFP, 0x07);
	set(0x21, 0x02);
	set(0x22, 0x91);
	set(0x29, 0x07);
	set(0x33, 0x0b);
	set(0x35, 0x0b);
	set(0x37, 0x1d);
	set(0x38, 0x71);
	set(0x39, 0x2a);
	set(REG_COM12, 0x78);
	set(0x4d, 0x40);
	set(0x4e, 0x20);
	set(REG_GFIX, 0);
	set(0x6b, 0x4a);
	set(0x74, 0x10);
	set(0x8d, 0x4f);
	set(0x8e, 0);
	set(0x8f, 0);
	set(0x90, 0);
	set(0x91, 0);
	set(0x96, 0);
	set(0x9a, 0);
	set(0xb0, 0x84);
	set(0xb1, 0x0c);
	set(0xb2, 0x0e);
	set(0xb3, 0x82);
	set(0xb8, 0x0a);

	/* Matrix coefficients */
	set(0x4f, 0x80);
	set(0x50, 0x80);
	set(0x51, 0);
	set(0x52, 0x22);
	set(0x53, 0x5e);
	set(0x54, 0x80);
	set(0x58, 0x9e);

	/* More reserved magic, some of which tweaks white balance */
	set(0x43, 0x0a);
	set(0x44, 0xf0);
	set(0x45, 0x34);
	set(0x46, 0x58);
	set(0x47, 0x28);
	set(0x48, 0x3a);
	set(0x59, 0x88);
	set(0x5a, 0x88);
	set(0x5b, 0x44);
	set(0x5c, 0x67);
	set(0x5d, 0x49);
	set(0x5e, 0x0e);
	set(0x6c, 0x0a);
	set(0x6d, 0x55);
	set(0x6e, 0x11);
	set(0x6f, 0x9f); /* "9e for advance AWB" */
	set(0x6a, 0x40);
	set(REG_BLUE, 0x40);
	set(REG_RED, 0x60);
	set(REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT | COM8_AGC | COM8_AEC | COM8_AWB);

	delay_ms(300);
}

uint16_t yuvto565(int y, int u, int v) {
	int c = y - 16;
	int d = u - 128;
	int e = v - 128;

	int r = (298*c + 409*e + 128) >> 8;
	int g = (298*c - 100*d - 208*e + 128) >> 8;
	int b = (298*c + 516*d + 128) >> 8;

	if (r < 0) r = 0; else if (r > 255) r = 255;
	if (g < 0) g = 0; else if (g > 255) g = 255;
	if (b < 0) b = 0; else if (b > 255) b = 255;

	return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint8_t reduce(uint16_t rgb565) {
	uint8_t r = (rgb565 >> 13) & 0x07;
	uint8_t g = (rgb565 >> 8)  & 0x07;
	uint8_t b = (rgb565 >> 3)  & 0x03;

	return (r << 5) | (g << 2) | b;
}

uint8_t collectByte() {
	while (!PCLK_READ()); // wait for PCLK to rise

	uint8_t v =
		(!!D0_READ()) << 0 |
		(!!D1_READ()) << 1 |
		(!!D2_READ()) << 2 |
		(!!D3_READ()) << 3 |
		(!!D4_READ()) << 4 |
		(!!D5_READ()) << 5 |
		(!!D6_READ()) << 6 |
		(!!D7_READ()) << 7;

	while (PCLK_READ()); // wait for PCLK to fall

	return v;
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
	delay_ms(300);
	size_t tp;
	uint8_t y;
	uint16_t col = 0;
	uint16_t row = 0;
	while (VS_READ()); // wait for vsync to drop
	do {
		while (!HS_READ()); // wait for href to rise
		col = 0;
		do {
			uint8_t y0 = collectByte();
			ledOn();
			if (!HS_READ()) break;
			uint8_t u  = collectByte();
			if (!HS_READ()) break;
			uint8_t y1 = collectByte();
			if (!HS_READ()) break;
			uint8_t v  = collectByte();

			// tp = col * 240 + row;
			tp = row * 240 + col;
			if (col < 240) {
				fb[tp]     = reduce(yuvto565(y0, u, v));
			}
			if (col + 1 < 240) {
				fb[tp + 1] = reduce(yuvto565(y1, u, v));
			}
			col += 2;
		} while(HS_READ());
		++row;
		if (row == 240) return;
	} while(!VS_READ());
}
