
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "sh1106/display.h"
#include "tetris.h"

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

void ledOff();
void ledOn();

uint8_t gfx[1024];

int main(void) {
	SysTick_Config(SystemCoreClock / 1000); // 1ms tick

	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	LED_INIT();
	BUTT_INIT();
	display_init();
	display_clear();
	tetris_init();

	// uint8_t frame[192];
	while (1) {
		tetris_update(gfx);
		display_updateTranslated(gfx);

		uint8_t button = !BUTT_READ();
		if (button) {
			ledOn();
		} else {
			ledOff();
		}
	}
}

void ledOff() {
	LED_HIGH();
}
void ledOn() {
	LED_LOW();
}