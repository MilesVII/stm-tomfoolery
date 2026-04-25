
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

	display_clear(0x00);

	/*
	B   Mag
	GR
	*/
	//RGB565
	uint32_t offset = 0;
	uint32_t pixelCount = display_setWindow(10, 10, 69, 32);
	for (int i = 0; i < pixelCount; ++i) {
		if (i % 69 < 32 && i < 69*16)
			GFX[i + offset] = 0xFFFF;
		else
			GFX[i + offset] = 0xFFFF;
	}
	display_sendBytes(GFX + offset, pixelCount);
	offset += pixelCount;

	display_setWindow(10, 42, 69, 32);
	for (int i = 0; i < pixelCount; ++i) {
		if (i % 69 < 32 && i < 69*16)
			GFX[i + offset] = COL_G;
		else
			GFX[i + offset] = 0xFFFF;
	}
	display_sendBytes(GFX + offset, pixelCount);
	offset += pixelCount;

	display_setWindow(79, 10, 69, 32);
	for (int i = 0; i < pixelCount; ++i) {
		if (i % 69 < 32 && i < 69*16)
			GFX[i + offset] = COL_B;
		else
			GFX[i + offset] = 0xFFFF;
	}
	display_sendBytes(GFX + offset, pixelCount);
	offset += pixelCount;

	// uint32_t pc = display_setWindow(120, 160, 120, 160);
	// 	for (int i = 0; i < pc; ++i) {
	// 	GFX[i] = (i % 7 == 1) ? (COL_R | COL_B) : 0;
	// }
	// display_sendBytes(GFX, pc);

	while (1) {
		delay_ms(32);

		uint8_t button = !GPIOA->IDR & (1 << 0);
		uint8_t touch = !touch_up();
		if (touch || button) {
			ledOn();
			if (touch) {
				uint32_t point = touch_poll();
				// uint16_t x = point >> 16;
				// uint16_t y = point & 0xFFFF;
				// uint16_t color = COL_G;
				// uint8_t flyoff = x > 200 || y > 200;
				// if (flyoff) {
					// x = 120;
					// y = 120;
				// 	color = COL_R;
				// }
				// display_setWindow(x, y, 2, 2);
				// GFX[0] = color;
				// GFX[1] = color;
				// GFX[2] = color;
				// GFX[3] = color;
				// display_sendBytes(GFX, 4);
			}
		} else {
			ledOff();
		}
	}
}
