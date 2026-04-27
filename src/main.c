
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "display.h"
#include "touch.h"

uint16_t GFX[120*160];
#define FLIP16(v) ((v) >> 8 | ((v) << 8) & 0xFF00)

void ledOff() {
	// C13 high
	GPIOC->ODR |= (1 << 13);
}
void ledOn() {
	// C13 low
	GPIOC->ODR &= ~(1 << 13);
}

void status(uint8_t halfColor) {
	display_clear(halfColor, 0, 0, 4, 4);
}

const uint16_t COL_R = 0b1111100000000000;
const uint16_t COL_G = 0b0000011111100000;
const uint16_t COL_B = 0b0000000000011111;

int main(void) {
	SysTick_Config(SystemCoreClock / 1000); // 1ms tick

	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	// PC13 led
	MODER(GPIOC, 13, 1)

	// A0 button
	// MODER(GPIOA, 13, 1)
	PUPDR(GPIOA, 0, 1);

	delay_ms(200);
	display_initSPI();
	delay_ms(200);
	display_init();
	touch_initSPI();

	display_clear(0x00, 0, 0, 240, 320);

	/*
	B   Mag
	GR
	*/
	//RGB565
	uint32_t offset = 0;
	uint32_t pixelCount = display_setWindow(10, 10, 69, 32);
	for (int i = 0; i < pixelCount; ++i) {
		GFX[i + offset] = 0x0000;
	}
	display_sendBytes(GFX + offset, pixelCount);
	offset += pixelCount;

	display_setWindow(10, 42, 69, 32);
	for (int i = 0; i < pixelCount; ++i) {
		if (i % 69 < 32 && i < 69*16)
			GFX[i + offset] = COL_G;
		else
			GFX[i + offset] = 0x0000;
	}
	display_sendBytes(GFX + offset, pixelCount);
	offset += pixelCount;

	display_setWindow(79, 10, 69, 32);
	for (int i = 0; i < pixelCount; ++i) {
		if (i % 69 < 32 && i < 69*16)
			GFX[i + offset] = COL_B;
		else
			GFX[i + offset] = 0x0000;
	}
	display_sendBytes(GFX + offset, pixelCount);
	offset += pixelCount;

	uint8_t cycles = 0;
	while (1) {
		// delay_ms(32);
		cycles = (cycles + 1) % 32;
		if (cycles == 0 || cycles == 16) {
			status(cycles ? 0xF0 : 0x00);
		}

		uint8_t button = !GPIOA->IDR & (1 << 0);
		uint8_t touch = !touch_up();
		if (button || touch) {
			ledOn();
			if (touch) {
				uint16_t x;
				uint16_t y;
				uint16_t lineOffset = DIGIT_H * 1.5;
				touch_poll(&x, &y);
				touch_poll(&x, &y);
				// display_number(GFX, x, 239 - 4, 319 - DIGIT_H - lineOffset * 0);
				// display_number(GFX, y, 239 - 4, 319 - DIGIT_H - lineOffset * 1);
				touch_calibrate(&x, &y);
				// display_number(GFX, x, 239 - 4, 319 - DIGIT_H - lineOffset * 2);
				// display_number(GFX, y, 239 - 4, 319 - DIGIT_H - lineOffset * 3);
				uint16_t pc = display_setWindow(x-1, y-1, 3, 3);
				for (uint16_t i = 0; i < pc; ++i) {
					GFX[i] = 0xF00D;
				};
				display_sendBytes(GFX, pc);
			}
		} else {
			ledOff();
		}
	}
}
