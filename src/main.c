
#include "stm32f411xe.h"

void delay_cycles(volatile uint32_t cycles) {
	while (cycles--) {
		__NOP();
	}
}

int main(void) {
	// enable GPIOC clock
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

	// PC13 led
	GPIOC->MODER &= ~(3 << (13 * 2));
	GPIOC->MODER |=  (1 << (13 * 2));

	// A0 button
	GPIOA->MODER &= ~(3 << (0 * 2));
	GPIOA->PUPDR &= ~(3 << (0 * 2));
	GPIOA->PUPDR |=  (1 << (0 * 2));

	while (1) {
		delay_cycles(1000000);
		if (GPIOA->IDR & (1 << 0)) {
			GPIOC->ODR |= (1 << 13);
		} else {
			GPIOC->ODR &= ~(1 << 13);
		}
	}
}
