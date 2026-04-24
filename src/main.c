
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "display.h"

uint16_t GFX[120*160];
#define FLIP16(v) ((v) >> 8 | ((v) << 8) & 0xFF00)

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
	GPIOA->PUPDR |=  (1 << (0 * 2));

	delay_ms(200);
	display_initSPI();
	delay_ms(200);
	display_init();

	uint8_t pstate = 0;
	uint8_t click = 0;
	int clickCounter = 0;

	/*
	B   Mag
	GR
	*/
	//RGB565
	uint32_t pixelCount = display_setWindow(10, 10, 69, 32);
	for (int i = 0; i < pixelCount; ++i) {
		GFX[i] = FLIP16((i % 2) ? 0b0000011111100000 : 0);
	}
	display_sendBytes(GFX, pixelCount);

	display_setWindow(10, 42, 69, 32);
	for (int i = 0; i < pixelCount; ++i) {
		GFX[pixelCount + i] = FLIP16((i % 2) ? 0b0000000000011111 : 0);
	}
	display_sendBytes(GFX + pixelCount, pixelCount);
	
	display_setWindow(79, 10, 69, 32);
	for (int i = 0; i < pixelCount; ++i) {
		GFX[pixelCount * 2 + i] = FLIP16((i % 2) ? 0b1111100000000000 : 0);
	}
	display_sendBytes(GFX + pixelCount * 2, pixelCount);
	
	uint32_t pc = display_setWindow(120, 160, 120, 160);
		for (int i = 0; i < pc; ++i) {
		GFX[i] = FLIP16((i % 4 > 1) ? 0b1111100000011111 : 0);
	}
	display_sendBytes(GFX, pc);

	while (1) {
		delay_ms(32);
		click = 0;

		pstate = GPIOA->IDR & (1 << 0);
		if (pstate) {
			// butt down
			// led high
			GPIOC->ODR |= (1 << 13);
			if (pstate == 0) click = 1;
		} else {
			// butt up
			// led low
			GPIOC->ODR &= ~(1 << 13);
		}

	}
}
