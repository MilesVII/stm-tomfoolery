
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

#define TARGET_FPS 60.0
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

	#define ST(row) SH - BH * ((row) + 1) - 1
	#define RECT_START   0, ST(0), SW, BH
	#define RECT_Q1 SW / 2, ST(1), SW / 2, BH
	#define RECT_Q2      0, ST(1), SW / 2, BH
	#define RECT_Q3      0, ST(2), SW / 2, BH
	#define RECT_Q4 SW / 2, ST(2), SW / 2, BH
	display1_button(gfx1, "START/PAUSE", RECT_START);
	display1_button(gfx1, "< LEFT", RECT_Q3);
	display1_button(gfx1, "RITE >", RECT_Q4);
	display1_button(gfx1, "@ SPIN", RECT_Q2);
	display1_button(gfx1, "+ DROP", RECT_Q1);
	// display1_number(gfx1, DTC() % 1000, 230, SH - BH - 1 - DIGIT_H);

	uint16_t touches[] = { 0, 0, 0, 0 };
	uint8_t touchCount;
	uint16_t gameIO;
	uint16_t score = TETRIS_SCORE_DIRTY;
	float smolMS = 0.0;
	float fullMS = 0.0;
	while (1) {
		DT_RESET();
		gameIO =
			(gameIO << 8) |
			(button_touch(touchCount, touches, RECT_Q1) ? TETRIS_IO_D : 0x00) |
			(button_touch(touchCount, touches, RECT_Q2) ? TETRIS_IO_S : 0x00) |
			(button_touch(touchCount, touches, RECT_Q3) ? TETRIS_IO_L : 0x00) |
			(button_touch(touchCount, touches, RECT_Q4) ? TETRIS_IO_R : 0x00) |
			(button_touch(touchCount, touches, RECT_START) ? TETRIS_IO_P : 0x00);

		tetris_update(gfx0, gameIO, fullMS, &score);
		display0_updateTranslated(gfx0);
		touch_poll(&touchCount, touches, SW);
		if (score & TETRIS_SCORE_DIRTY) {
			score = score & ~TETRIS_SCORE_DIRTY;
			display1_number(gfx1, score, SW, 0);
		}

		uint8_t button = !BUTT_READ();
		if (button) {
			ledOn();
		} else {
			ledOff();
		}

		// game update
		smolMS = DT();
		if (smolMS < targetFrameTimeMS) {
			float sleepTimeMS = roundf(targetFrameTimeMS - smolMS);
			if (sleepTimeMS > 0) delay_ms((uint32_t)sleepTimeMS);
		} else {
			ledOn();
		}
		fullMS = DT();
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
