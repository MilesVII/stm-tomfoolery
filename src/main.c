
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "display.h"
#include "thermal.h"
#include "badapol.h"

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

int main(void) {
	SysTick_Config(SystemCoreClock / 1000); // 1ms tick

	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	// PC13 led
	MODER(GPIOC, 13, 1)

	// A0 button
	// MODER(GPIOA, 13, 1)
	GPIOA->PUPDR |=  (1 << (0 * 2));

	delay_ms(200);
	display_initSPI();
	delay_ms(200);
	display_init();
	delay_ms(500);
	display_clear();
	delay_ms(200);
	thermal_initI2C();

	uint8_t pstate = 0;
	uint8_t click = 0;
	uint16_t temp = 0;
	int clickCounter = 0;
	int frame = 0;

	while (1) {
		delay_ms(32);
		click = 0;

		pstate = GPIOA->IDR & (1 << 0);
		if (pstate) {
			// butt down
			// led high
			GPIOC->ODR |= (1 << 13);
			temp = thermal_poll() * 2;
			if (pstate == 0) click = 1;
		} else {
			// butt up
			// led low
			GPIOC->ODR &= ~(1 << 13);
		}

		if (click) {
			++clickCounter;
		}
		uint8_t* frameData = readFrame(&frame);
		display_update_48_32(frameData, frame, temp - 27315);
	}
}
