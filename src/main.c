
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
void line(uint16_t v, uint16_t line) {
	const uint16_t lineOffset = DIGIT_H * 1.5;
	display_number(GFX, v, 239 - 4, 319 - DIGIT_H - lineOffset * line);
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
	touch_initI2C();

	display_clear(0x00, 0, 0, 240, 320);

	uint8_t cycles = 0;
	uint8_t touchCount = 0;
	uint16_t touches[4] = { 0, 0, 0, 0 };
	while (1) {
		delay_ms(32);
		cycles = (cycles + 1) % 64;
		if (cycles == 0 || cycles == 8) {
			status(cycles ? 0x00 : 0xF0);
		}

		uint8_t button = !GPIOA->IDR & (1 << 0);
		uint8_t touch = !touch_up();
		if (button || touch) {
			ledOn();
			if (touch) {
				touch_poll(&touchCount, touches);

				if (touchCount >= 1) {
					line(touches[0], 1);
					line(touches[1], 1);
				}
				if (touchCount >= 2) {
					line(touches[2], 1);
					line(touches[3], 1);
				}
			} else {
				touchCount = 0;
			}
			line(touchCount, 0);
		} else {
			ledOff();
		}
	}
}
