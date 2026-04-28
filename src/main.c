
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "sh1106/display.h"

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

void update(uint16_t offset);
int patternA(int x);
int patternB(int x);
uint8_t pattern(int x, int y);
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

	uint16_t offset = 0;
	uint8_t frame[192];
	while (1) {
		delay_ms(16);

		update(offset);
		display_updateTranslated(gfx);
		uint8_t button = !BUTT_READ();
		if (button) {
			++offset;
			ledOn();
		} else {
			ledOff();
		}
	}
}

void update(uint16_t offset) {
	for (uint16_t i = 0; i < 1024; ++i) {
		uint8_t target = 0;
		uint16_t y = i / 8;
		uint16_t xB = i - y * 8;
		for (uint8_t bit = 0; bit < 8; ++bit) {
			uint16_t x = xB * 8 + bit;
			target |= pattern(x + offset * 2, y) << bit;
		}
		gfx[i] = target;
	}
}

int patternA(int x){
	return ((x*x)&x);
}
int patternB(int x){
	return (x>>7)&x;
}
uint8_t pattern(int x, int y) {
	int v = x ^ y;
	return patternA(v) > patternB(v) ? 0 : 1;
}
void ledOff() {
	LED_HIGH();
}
void ledOn() {
	LED_LOW();
}