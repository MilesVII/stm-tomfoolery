
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "ili9341/display.h"
#include "ft6336g/touch.h"

uint16_t GFX[120*160];
// #define FLIP16(v) ((v) >> 8 | ((v) << 8) & 0xFF00)

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

void ledOff() {
	LED_HIGH();
}
void ledOn() {
	LED_LOW();
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

	LED_INIT();
	BUTT_INIT();

	display_init(1);
	touch_init();

	display_clear(0x00, 0, 0, 240, 320);

	uint8_t cycles = 0;
	uint8_t touchCount = 0;
	uint16_t touches[4] = { 0, 0, 0, 0 };
	while (1) {
		delay_ms(32);
		cycles = (cycles + 1) % 32;
		if (cycles == 0 || cycles == 16) {
			status(cycles ? 0x0F : 0xF0);
		}

		touch_poll(&touchCount, touches);
		line(touchCount, 0);
		uint8_t button = !BUTT_READ();
		if (button || touchCount > 0) {
			ledOn();
			if (touchCount >= 1) {
				uint16_t x = 239 - touches[0];
				uint16_t y = touches[1];
				line(x, 1);
				line(y, 2);
				display_clear(0xFF, x-1, y-1, 3, 3);
			}
			if (touchCount >= 2) {
				uint16_t x = 239 - touches[2];
				uint16_t y = touches[3];
				line(x, 3);
				line(y, 4);
				display_clear(0xFF, x-1, y-1, 3, 3);
			}
		} else {
			ledOff();
		}
	}
}
