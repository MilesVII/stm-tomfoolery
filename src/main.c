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
DECLARE_GPIO_MOUT(POWER, A, 2);
#define BATT_DIV_R1 10.91f
#define BATT_DIV_R2 32.8f
#define BATT_DIV_RATIO ((BATT_DIV_R1 + BATT_DIV_R2) / BATT_DIV_R1)

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
	float power;
	float holdM;
	float holdP;
);

// ms
Timings TIMING_CONFIG = {
	.thermalPoll = 2000,
	.touchPoll = 100,
	.batteryPoll = 2000,
	.power = 4200,
	.holdM = 160,
	.holdP = 160,
};
Timings timings = { 0, 0, 0, 0, 0 };
uint8_t powerState = 0;
#define TIMING_POWER_IDLE 200

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
	POWER_INIT(); POWER_HIGH();

	display1_init(1);
	display1_clear(0x00, 0, 0, SW, SH);
	touch_init();
	thermal_init();
	display1_button(gfx, "-", 0x0000, RECT_M);
	display1_button(gfx, "+", 0x0000, RECT_P);

	display1_stringCentered(gfx, "rgbcmy  gONLINE", 0x0000, 1, RECT_L(2));

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

		timings.power += dt;
		if (timings.power > TIMING_CONFIG.power) {
			timings.power = 0;
		}
		if (timings.power < TIMING_POWER_IDLE && powerState) {
			powerState = 0;
			POWER_HIGH();
			LED_HIGH();
			display1_stringCentered(gfx, "yIDLE", 0x0000, 1, RECT_L(2));
		}
		if (timings.power > TIMING_POWER_IDLE && !powerState) {
			// poll battery
			float v = BATT_READ() * BATT_DIV_RATIO;
			sprintf(gaugeCaption, "VOLTAGE: %5.2fV / %.1f%%", v, (v - 7.0f) / 1.4f * 100);
			display1_stringCentered(gfx, gaugeCaption, 0x0000, 1, RECT_L(1));

			// poll temp
			float t = thermal_poll();
			sprintf(tempCaption, "READING: %5.1f `C", t);
			display1_stringCentered(gfx, tempCaption, 0x0000, 1, RECT_L(0));

			powerState = 1;
			if (t < target) {
				POWER_LOW();
				LED_LOW();
				display1_stringCentered(gfx, "gHI", 0x0000, 1, RECT_L(2));
			} else
				display1_stringCentered(gfx, "rLO", 0x0000, 1, RECT_L(2));
		}

		dt = DT();
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
