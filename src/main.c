
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "sh1106/display.h"
#include <stdlib.h>
#include <math.h>
#include "frames.h"

#define STRUCT(name, fields) \
	struct name { \
		fields \
	}; \
	typedef struct name name;
#define ENUM(name, ...) \
	enum name { \
		__VA_ARGS__ \
	}; \
	typedef enum name name;
#define FILL(v, d) for (int i = 0; i < (sizeof(v) / sizeof(v[0])); ++i) { v[i] = d; }
#define AT(x, y, w) ((x) + (y) * (w))

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

uint8_t gfx[1024];
void update(float pdt, uint8_t button);
void drawFrame(uint8_t frameIx, uint8_t x, uint8_t y);
uint8_t framedata[] = { FRAMEDATA };

void ledOff() {
	LED_HIGH();
}
void ledOn() {
	LED_LOW();
}

void ADCT_init() {
	// ADC1 clock
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

	ADC->CCR |= ADC_CCR_TSVREFE;
	ADC1->CR1 = 0;
	ADC1->CR2 = 0;

	// sample time
	ADC1->SMPR1 |= (0x0 << ADC_SMPR1_SMP18_Pos);

	ADC1->SQR3 = 18; // Channel 18 = temperature sensor
	ADC1->SQR1 = 0;  // L = 0 → 1 conversion

	ADC1->CR2 |= ADC_CR2_ADON;
}


uint16_t ADCT_read() {
	// Start conversion
	ADC1->CR2 |= ADC_CR2_SWSTART;

	// Wait for End Of Conversion flag
	while (!(ADC1->SR & ADC_SR_EOC));

	return (uint16_t)ADC1->DR;
}

int main(void) {
	SysTick_Config(SystemCoreClock / 1000); // 1ms tick

	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	ADCT_init();
	srand(ADCT_read());
	LED_INIT();
	BUTT_INIT();

	display0_init();

	DWTCC_INIT();
	while (1) {
		float pdt = DWTCC_DT();
		DWTCC_DT_RESET();
		update(pdt, !BUTT_READ());
		display0_updateTranslated(gfx);
	}
}

#define REEL_COUNT 3
#define REEL_STOPS 4
#define MAX_STOP_TIME_MS 3200
#define ALIGN_EPSILON .01
#define ALIGN_STOP_TIME_MS 400
#define WIN_BLINK_TIME_MS 400
#define REEL_SPIN_FACTOR .2
STRUCT(State,
	float offsets[REEL_COUNT];
	float spin[REEL_COUNT];
	uint8_t running;
	uint8_t init;
	uint8_t win;
	float winTime;
);
State state = {
	.init = 0,
	.win = 0,
	.winTime = 0.0
};
uint8_t reelStopIndesex[REEL_COUNT * REEL_STOPS];

const float STOP_WIDTH = 1.0 / REEL_STOPS;
inline float alignStop(float offset) {
	return STOP_WIDTH * floor(offset / STOP_WIDTH);
}

void drawFrame(uint8_t frameIx, uint8_t x, uint8_t y) {
	uint8_t bitoffset = x % 8;
	uint8_t bitrevers = 8 - bitoffset;
	uint8_t pages = bitoffset == 0 ? 2 : 3;
	uint8_t page0 = x / 8;
	for (uint8_t line = 0; line < 16; ++line) {
		uint8_t b0 = framedata[FD_BPF * frameIx + AT(0, line, 2)];
		uint8_t b1 = framedata[FD_BPF * frameIx + AT(1, line, 2)];
		if (pages == 2) {
			uint8_t page1 = (page0 + 1) % 8;
			gfx[AT(page0, line + y, 8)] = b0;
			gfx[AT(page1, line + y, 8)] = b1;
		} else {
			uint8_t page1 = (page0 + 1) % 8;
			uint8_t page2 = (page0 + 2) % 8;
			gfx[AT(page0, line + y, 8)] |= b0 << bitoffset;
			gfx[AT(page1, line + y, 8)] |= (b0 >> bitrevers) | (b1 << bitoffset);
			gfx[AT(page2, line + y, 8)] |= b1 >> bitrevers;
		}
	}
}

void update(float pdt, uint8_t button) {
	if (!state.init) {
		for (int rix = 0; rix < REEL_COUNT; ++rix) {
			state.offsets[rix] = (rand() % 1000) / 1000.0;
			state.offsets[rix] = alignStop(state.offsets[rix]);
			state.spin[rix] = 0.0;
			for (int i = 0; i < REEL_STOPS; ++i) {
				reelStopIndesex[i + rix * REEL_STOPS] = i;
			}

			// shuffle
			uint8_t offset = rix * REEL_STOPS;
			for (int i = REEL_STOPS - 1; i >= 0; --i) {
				int j = rand() % (i + 1);
				uint8_t pocket = reelStopIndesex[offset + i];
				reelStopIndesex[offset + i] = reelStopIndesex[offset + j];
				reelStopIndesex[offset + j] = pocket;
			}
		}

		state.running = 0;
		state.init = 1;
	}

	if (state.running < REEL_COUNT && button) {
		state.running = REEL_COUNT;
		state.win = 0;
		for (int i = 0; i < REEL_COUNT; ++i) {
			state.spin[i] = (rand() % 1000) / 1000.0 * .5 + .5;
		}
	}

	uint8_t check = 0;
	if (state.running) {
		float unused;
		for (int i = 0; i < REEL_COUNT; ++i) {
			if (state.spin[i] == -1) continue;
			if (state.spin[i] == 0) {
				float target = alignStop(state.offsets[i]);
				float delta = target - state.offsets[i];
				state.offsets[i] += delta * pdt / ALIGN_STOP_TIME_MS;
				if (fabs(delta) < ALIGN_EPSILON) {
					--state.running;
					state.spin[i] = -1;
					state.offsets[i] = target;
					if (state.running == 0) {
						check = 1;
						break;
					}
				}
				continue;
			}

			state.offsets[i] += state.spin[i] * REEL_SPIN_FACTOR;
			state.offsets[i] = modff(state.offsets[i], &unused);
			state.spin[i] -= pdt / MAX_STOP_TIME_MS;
			if (state.spin[i] <= 0) {
				state.spin[i] = 0;
			}
		}
	}

	if (check) {
		// check win condition there
		uint8_t stops[REEL_COUNT];
		for (uint8_t i = 0; i < REEL_COUNT; ++i) {
			uint8_t displacement = (uint8_t)floor(state.offsets[i] * 64);
			uint8_t stop = ((24 - displacement + 64) % 64) / 16;
			stops[i] = reelStopIndesex[i * REEL_STOPS + stop];
		}
		for (uint8_t i = 1; i < REEL_COUNT; ++i) {
			if (stops[i] != stops[i - 1]) goto LOSE;
		}
		state.win = 1;
		LOSE:
		check = 0;
	}

	if (state.win) {
		state.winTime += pdt;
		while (state.winTime > WIN_BLINK_TIME_MS) state.winTime -= WIN_BLINK_TIME_MS;
		if (state.winTime > WIN_BLINK_TIME_MS / 2.0) ledOn(); else ledOff();
	} else {
		state.winTime = 0;
	}

	//render
	FILL(gfx, 0x00);
	for (int reel = 0; reel < REEL_COUNT; ++reel)
	for (int stop = 0; stop < REEL_STOPS; ++stop) {
		uint8_t displacement = (uint8_t)floor(state.offsets[reel] * 64);
		drawFrame(
			reelStopIndesex[reel * REEL_STOPS + stop],
			(stop * 16 + displacement + 8) % 64,
			reel * 16 + (128 - 16 * REEL_COUNT) / 2
		);
	}
	// arrow
	uint8_t arrowY = (128 - REEL_COUNT * 16) / 2 - 8 + 2;
	for (int i = 0; i < 8; ++i) {
		gfx[AT(3, arrowY + i, 8)] |= (0xFF << i);
		gfx[AT(4, arrowY + i, 8)] |= (0xFF >> i);
	}
	if (state.winTime > WIN_BLINK_TIME_MS / 2.0) {
		FILL(gfx, ~gfx[i]);
	}
}
