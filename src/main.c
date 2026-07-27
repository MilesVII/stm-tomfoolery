
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "ili9341/display.h"

// #define FLIP16(v) ((v) >> 8 | ((v) << 8) & 0xFF00)

extern const uint8_t picData[];
extern const uint8_t picEnd[];
const uint16_t* palette = (uint16_t*)picData; // 256 2-byte colors
const uint8_t* picture = picData + 512;

uint16_t gfxPalette[256];
uint8_t gfx[240*320];

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

void ledOff() {
	LED_HIGH();
}
void ledOn() {
	LED_LOW();
}

void status(uint8_t halfColor) {
	display1_clear(halfColor, 0, 0, 4, 4);
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

	for (uint16_t i = 0; i < 256; ++i) gfxPalette[i] = hydrate(i);

	LED_INIT();
	BUTT_INIT();

	display1_init(0);
	display1_sendBytesIndexed(picture, palette, 240 * 320);

	while (1) {
		delay_ms(32);
		if (BUTT_READ()) ledOn(); else ledOff();
	}
}
