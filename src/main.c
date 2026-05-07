
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "sh1106/display.h"
#include "ili9341/display.h"
#include "ft6336g/touch.h"
#include "tetris.h"
#include <math.h>
#include <stdlib.h>

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

void ledOff();
void ledOn();
uint8_t button_touch(
	uint8_t count, uint16_t* touches,
	uint16_t x, uint16_t y, uint16_t w, uint16_t h
);
// screen size
#define SW 240
#define SH 320
// button height
#define BH 64

#define TARGET_FPS 120.0
const float targetFrameTimeMS = 1000.0 / TARGET_FPS;
#define DT() (float)DWT->CYCCNT * 1000.0 / SystemCoreClock
#define DTC() DWT->CYCCNT
#define DT_RESET() DWT->CYCCNT = 0

uint8_t gfx0[1024];
uint16_t gfx1[SW * SH / 4];

int main(void) {
	SysTick_Config(SystemCoreClock / 1000); // 1ms tick

	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;
	
	// init DWT cycle counter
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

	srand(0); // TODO: use actual enthropy sources

	LED_INIT();
	BUTT_INIT();
	display0_init();
	display1_init(1);
	touch_init();

	display0_clear();
	display1_clear(0x00, 0, 0, SW, SH);
	tetris_init(gfx0);

	#define RECT_START 0, SH - BH - 1, SW, BH
	#define RECT_Q1 SW / 2, BH, SW / 2, BH
	#define RECT_Q2      0, BH, SW / 2, BH
	#define RECT_Q3      0,  0, SW / 2, BH
	#define RECT_Q4 SW / 2,  0, SW / 2, BH
	display1_button(gfx1, "START/PAUSE", RECT_START);
	display1_button(gfx1, "< LEFT", RECT_Q3);
	display1_button(gfx1, "RITE >", RECT_Q4);
	display1_button(gfx1, "@ SPIN", RECT_Q2);
	display1_button(gfx1, "+ DROP", RECT_Q1);
	// display1_number(gfx1, DTC() % 1000, 230, SH - BH - 1 - DIGIT_H);

	uint16_t touches[] = { 0, 0, 0, 0 };
	uint8_t touchCount;
	uint8_t gameIO;
	float timeMS = 0.0;
	float gameMS = 0.0;
	float tuchMS = 0.0;
	float dispMS = 0.0;
	DT_RESET();
	while (1) {
		gameIO =
			(button_touch(touchCount, touches, RECT_Q1) ? TETRIS_IO_D : 0x00) |
			(button_touch(touchCount, touches, RECT_Q2) ? TETRIS_IO_S : 0x00) |
			(button_touch(touchCount, touches, RECT_Q3) ? TETRIS_IO_L : 0x00) |
			(button_touch(touchCount, touches, RECT_Q4) ? TETRIS_IO_R : 0x00);

		DT_RESET();
		tetris_update(gfx0, gameIO, timeMS);
		gameMS = DT();

		DT_RESET();
		display0_updateTranslated(gfx0);
		dispMS = DT();

		DT_RESET();
		// touch_poll(&touchCount, touches, SW);
		tuchMS = DT();

		uint8_t button = !BUTT_READ();
		if (button) {
			ledOn();
		} else {
			ledOff();
		}

		// game update
		display1_number(gfx1, (uint16_t)roundf(gameMS* 1000), 230, SH - BH - 1 - DIGIT_H);
		// display update
		display1_number(gfx1, (uint16_t)roundf(dispMS * 1000), 230, SH - BH - 1 - DIGIT_H * 2);
		// touch poll
		display1_number(gfx1, (uint16_t)roundf(tuchMS * 1000), 230, SH - BH - 1 - DIGIT_H * 3);

		// timeMS = DT();
		// DT_RESET();
		// if (timeMS < targetFrameTimeMS) {
		// 	float sleepTimeMS = roundf(targetFrameTimeMS - timeMS);
		// 	if (sleepTimeMS > 0) delay_ms((uint32_t)sleepTimeMS);
		// } else {
		// 	ledOn();
		// }
	}
}

void ledOff() {
	LED_HIGH();
}
void ledOn() {
	LED_LOW();
}

uint8_t button_touch(
	uint8_t count, uint16_t* touches,
	uint16_t x, uint16_t y, uint16_t w, uint16_t h
) {
	if (
		count >= 1 &&
		touches[0] >= x && touches[0] < (x + w) &&
		touches[1] >= y && touches[1] < (y + h)
	) return 1;
	if (
		count >= 2 &&
		touches[1] >= x && touches[1] < (x + w) &&
		touches[2] >= y && touches[2] < (y + h)
	) return 1;
	return 0;
}
