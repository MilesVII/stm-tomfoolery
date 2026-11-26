
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "plex.h"

DECLARE_GPIO_MIN(BUTT, A, 0);
DECLARE_GPIO_MOUT(LED, C, 13);

void ledOff() {
	LED_HIGH();
}
void ledOn() {
	LED_LOW();
}

void systick_hook(void) {
	plex_tick();
}

int main(void) {
	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOCEN;

	plex_init();
	LED_INIT();
	BUTT_INIT();

	SysTick_Config(SystemCoreClock / 1000); // 1ms tick, also drives the display

	uint8_t n = 0;
	uint8_t bp = 0;
	plex_show(n, false, true);

	while (1) {
		delay_ms(32);
		uint8_t bc = !BUTT_READ();
		if (bc && !bp) {
			if (++n >= 200) n = 0;
			plex_show(n, n & 1, true);
		}
		bp = bc;
	}
}
