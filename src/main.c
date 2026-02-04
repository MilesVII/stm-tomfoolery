
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "display.h"

int main(void) {
	SysTick_Config(SystemCoreClock / 1000); // 1ms tick

	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	// PC13 led
	MODER(GPIOC, 13, 1)

	// A0 button
	MODER(GPIOA, 13, 1)
	GPIOA->PUPDR |=  (1 << (0 * 2));

	delay_ms(200);
	display_initSPI();
	delay_ms(200);
	display_init();
	delay_ms(200);
	display_clear();
	delay_ms(200);

	while (1) {
		delay_ms(200);
		display_update();
		if (GPIOA->IDR & (1 << 0)) {
			GPIOC->ODR &= ~(1 << 13);
		} else {
			GPIOC->ODR |= (1 << 13);
		}
	}
}
