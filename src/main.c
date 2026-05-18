
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "sh1106/display.h"
#include <stdint.h>

#define BYTES_PER_FRAME 192
#define RLE_MARK 0xFE

extern const uint8_t rleData[];
extern const uint8_t rleEnd[];
#define RLE_LENGTH (uint32_t)(rleEnd - rleData);
#define TARGET_FPS 10.0
const float TARGET_FRAMETIME_MS = 1000.0 / TARGET_FPS;

uint8_t frame0[BYTES_PER_FRAME];
uint8_t frame1[BYTES_PER_FRAME];
// write destination frame
uint8_t activeFrame = 0;
// flag indicating the NEXT_FRAME was filled
uint8_t frameSwitch = 0;
#define NEXT_FRAME (activeFrame ? 0 : 1)

uint32_t frameCursor = 0;
uint8_t pushByte(uint8_t value) {
	if (activeFrame == 0)
		frame0[frameCursor] = value;
	else
		frame1[frameCursor] = value;
	++frameCursor;

	if (frameCursor >= BYTES_PER_FRAME) {
		frameCursor = 0;
		activeFrame = NEXT_FRAME;
		frameSwitch = 1;
	}
}

uint32_t rleCursor = 0;
uint8_t* readFrame(int* frameCounter) {
	while (1) {
		uint8_t byte = rleData[rleCursor];
		if (byte == RLE_MARK) {
			uint8_t value = rleData[rleCursor + 1];
			uint8_t seq   = rleData[rleCursor + 2];
			for (int i = 0; i < seq; ++i) {
				pushByte(value);
			}
			rleCursor += 3;
		} else {
			pushByte(byte);
			rleCursor += 1;
		}

		rleCursor %= RLE_LENGTH;

		if (frameSwitch) {
			if (rleCursor == 0) 
				*frameCounter = 0;
			else
				*frameCounter += 1;
			frameSwitch = 0;
			return NEXT_FRAME ? frame1 : frame0;
		}
	}
}

DECLARE_GPIO_MOUT(LED, C, 13);
// DECLARE_GPIO_MIN(BUTT, A, 0);

void ledOff() {
	LED_HIGH();
}
void ledOn() {
	LED_LOW();
}

int main(void) {
	SysTick_Config(SystemCoreClock / 1000); // 1ms tick

	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	display0_init();
	display0_clear();
	DWTCC_INIT();

	int frame = 0;

	while (1) {
		DWTCC_DT_RESET();
		uint8_t* frameData = readFrame(&frame);
		display0_update_48_32(frameData, frame, 0);
		float frameTime = DWTCC_DT();
		frameTime = TARGET_FRAMETIME_MS - frameTime;
		if (frameTime > 0) delay_ms(frameTime);
	}
}
