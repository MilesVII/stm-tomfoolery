#include <stdio.h>
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "ili9341/display.h"
#include "ft6336g/touch.h"
#include "max6675/thermal.h"
#include "ui.h"

uint16_t gfx[SW * SH / 4];
uint8_t io = 0;
#define DT() (float)DWT->CYCCNT * 1000.0 / SystemCoreClock
#define DTC() DWT->CYCCNT
#define DT_RESET() DWT->CYCCNT = 0

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);
DECLARE_ADC(BATT, A, 1, ADC1);
// DECLARE_GPIO_MIN(MOSFET, A, 1);

#define STRUCT(name, fields) \
	struct name { \
		fields \
	}; \
	typedef struct name name;

uint8_t button_touch(
	uint8_t count, uint16_t* touches,
	uint16_t x, uint16_t y, uint16_t w, uint16_t h
);

STRUCT(Timings,
	float thermalPoll;
	float touchPoll;
	float batteryPoll;
	float holdM;
	float holdP;
);

// ms
Timings TIMING_CONFIG = {
	.thermalPoll = 2000,
	.touchPoll = 100,
	.batteryPoll = 12000,
	.holdM = 160,
	.holdP = 160,
};
Timings timings = { 0, 0, 0, 0, 0 };

#define BTN_R(K, c) \
	if (IO_TAP(io, K)) display1_button(gfx, c, 0x1F06, RECT_##K); \
	else if(IO_RSE(io, K)) display1_button(gfx, c, 0x0000, RECT_##K);

#define ADJ_TARGET(delta) \
	target += delta; \
	updateTargetCaption = 1;

#define CONTROL(io, K, delta) \
	if (IO_HLD(io, K)) { \
		timings.hold##K += dt; \
		if (timings.hold##K > TIMING_CONFIG.hold##K) { \
			ADJ_TARGET(delta); \
			timings.hold##K -= TIMING_CONFIG.hold##K; \
		} \
	} else { \
		timings.hold##K = 0; \
		if (IO_TAP(io, K)) { \
			ADJ_TARGET(delta); \
		} \
	}

int main(void) {
	SysTick_Config(SystemCoreClock / 1000); // 1ms tick

	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

	LED_INIT();
	BUTT_INIT();
	BATT_INIT();
	// MOSFET_INIT(); MOSFET_HIGH();

	display1_init(1);
	display1_clear(0x00, 0, 0, SW, SH);
	touch_init();
	thermal_init();
	display1_button(gfx, "-", 0x0000, RECT_M);
	display1_button(gfx, "+", 0x0000, RECT_P);

	// uint32_t status[1] = { 0.0f };
	uint16_t touches[] = { 0, 0, 0, 0 };
	uint8_t touchCount;
	char targetCaption[8];
	char tempCaption[24];
	char gaugeCaption[64];
	uint16_t target = 100;
	uint8_t updateTargetCaption = 1;
	float dt = 0;
	while (1) {
		DT_RESET();
		touch_poll(&touchCount, touches, SW);
		if (touchCount > 2) touchCount = 0;
		IO_UPDATE(io,
			button_touch(touchCount, touches, RECT_M),
			button_touch(touchCount, touches, RECT_P)
		);
		BTN_R(M, "-");
		BTN_R(P, "+");

		CONTROL(io, M, -5);
		CONTROL(io, P, +5);

		if (updateTargetCaption) {
			sprintf(targetCaption, "%u `C", target);
			display1_stringCentered(gfx, targetCaption, 0x0000, 1, RECT_TARG);
			updateTargetCaption = 0;
		}

		timings.thermalPoll += dt;
		if (timings.thermalPoll > TIMING_CONFIG.thermalPoll) {
			timings.thermalPoll -= TIMING_CONFIG.thermalPoll;
			float t = thermal_poll();
			sprintf(tempCaption, "READING: %5.1f `C", t);
			display1_stringCentered(gfx, tempCaption, 0x0000, 1, RECT_TRED);
		}
		timings.batteryPoll += dt;
		if (timings.batteryPoll > TIMING_CONFIG.batteryPoll) {
			timings.batteryPoll -= TIMING_CONFIG.batteryPoll;
			float v = BATT_READ() * 4.0f;
			sprintf(gaugeCaption, "VOLTAGE: %5.3fV / %.1f%%", v, (v - 7.0f) / 1.4f * 100);
			display1_stringCentered(gfx, gaugeCaption, 0x0000, 1, RECT_BATT);
		}

		dt = DT();
		// status[0] = (uint32_t)(t * 100);
	}
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
