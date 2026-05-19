
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

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

uint8_t gfx[1024];
void update(float pdt, uint8_t button);
const uint8_t framedata[] = { FRAMEDATA };

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

	// ADCT_init();
	// srand(ADCT_read());
	LED_INIT();
	BUTT_INIT();

	display0_init();
	display0_clear();

	for (int i = 0; i < 32; ++i) {
		gfx[i % 2 + i / 2 * 8] = framedata[i];
	}
	for (int i = 0; i < 32; ++i) {
		gfx[i % 2 + i / 2 * 8 + 2] = framedata[i+FD_BPF*1];
	}
	for (int i = 0; i < 32; ++i) {
		gfx[i % 2 + i / 2 * 8 + 4] = framedata[i+FD_BPF*2];
	}
	for (int i = 0; i < 32; ++i) {
		gfx[i % 2 + i / 2 * 8 + 6] = framedata[i+FD_BPF*3];
	}

	DWTCC_INIT();
	while (1) {
		float pdt = DWTCC_DT();
		DWTCC_DT_RESET();
		// update(pdt);
		display0_updateTranslated(gfx);
		if (BUTT_READ()) ledOn(); else ledOff();
	}
}

STRUCT(State,
	float offsets[3];
	float spin[3];
	uint8_t running;
	uint8_t init;
);

State state = {
	.init = 0
};

#define MAX_STOP_TIME_MS 3200
#define ALIGN_STOP_TIME_MS 800

const float STOP_WIDTH = 1.0 / FD_COUNT;
inline float alignStop(float offset) {
	return STOP_WIDTH * floor(offset / STOP_WIDTH);
}

void update(float pdt, uint8_t button) {
	if (!state.init) {
		for (int i = 0; i < 3; ++i) {
			state.offsets[i] = (rand() % 1000) / 1000.0;
			state.offsets[i] = alignStop(state.offsets[i]);
			state.spin[i] = 0.0;
		}
		state.running = 0;
	}

	if (!state.running && button) {
		state.running = 3;
		for (int i = 0; i < 3; ++i) {
			state.spin[i] = (rand() % 1000) / 1000.0 * 16.0;
		}
	}

	if (state.running) {
		float unused;
		float slowdown = pdt / MAX_STOP_TIME_MS;
		for (int i = 0; i < 3; ++i) {
			if (state.spin[i] == 0) {
				float target = alignStop(state.offsets[i]);
				float delta = target - state.offsets[i];
				state.offsets[i] += delta;
			}

			state.offsets[i] += state.spin[i];
			state.offsets[i] = modff(state.offsets[i], &unused);
			state.spin[i] -= slowdown;
			if (state.spin[i] <= 0) {
				state.spin[i] = 0;
			}
		}
		
	}

}