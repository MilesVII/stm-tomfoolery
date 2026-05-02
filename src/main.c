
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "sh1106/display.h"
#include "ili9341/display.h"
#include "tetris.h"

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

void ledOff();
void ledOn();

uint8_t gfx0[1024];
uint16_t gfx1[120*160];

int main(void) {
	SysTick_Config(SystemCoreClock / 1000); // 1ms tick

	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	LED_INIT();
	BUTT_INIT();
	display0_init();
	display1_init(1);
	display0_clear();
	display1_clear(0x00, 0, 0, 240, 320);
	tetris_init();

	display1_string(gfx1, "ABCDEFGH", 10, 40);
	display1_string(gfx1, "IJKLMNOP", 10, 25);
	display1_string(gfx1, "QRSTUVWXYZ", 10, 10);
	// uint8_t frame[192];
	while (1) {
		tetris_update(gfx0);
		display0_updateTranslated(gfx0);

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