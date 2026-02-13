
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "display.h"
#include "thermal.h"
#include "badapol.h"

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
	delay_ms(500);
	display_clear();
	delay_ms(200);
	thermal_initI2C();

	uint8_t pstate = 0;
	uint8_t click = 0;
	uint16_t temp = 0;
	int clickCounter = 0;
	int frame = 0;

	while (1) {
		delay_ms(32);
		click = 0;

		pstate = GPIOA->IDR & (1 << 0);
		if (pstate) {
			// butt down
			// led high
			GPIOC->ODR |= (1 << 13);
			temp = thermal_poll() / 50;
			if (pstate == 0) click = 1;
		} else {
			// butt up
			// led low
			GPIOC->ODR &= ~(1 << 13);
		}

		if (click) {
			++clickCounter;
		}

		display_update_48_32(frameData + frame * BYTES_PER_FRAME, frame, temp - 273);
		if (++frame >= FRAME_COUNT) {
			frame = 0;
		}
	}
}
