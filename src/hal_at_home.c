#include "stm32f411xe.h"
#include "hal_at_home.h"

volatile uint32_t ticks;

void SysTick_Handler(void) {
	ticks++;
}

void delay_ms(uint32_t ms) {
	uint32_t start = ticks;
	while ((ticks - start) < ms) {
		__NOP();
	};
}
