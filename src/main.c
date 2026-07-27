
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "ili9341/display.h"
#include "flash25q64.h"

extern const uint8_t picData[];
extern const uint8_t picEnd[];
const uint16_t* palette = (uint16_t*)picData; // 256 2-byte colors
const uint8_t* picture = picData + 512;

#define PIC_WIDTH   240
#define PIC_HEIGHT  320
#define PIC_PIXELS  ((uint32_t)PIC_WIDTH * PIC_HEIGHT)
#define FLASH_PIC_ADDR 0x00000000

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

static void ledOff() {
	LED_HIGH();
}
static void ledOn() {
	LED_LOW();
}

static void decode_and_store_picture(void) {
	// Decode the indexed picture to RGB565 and write it to external flash.
	// We use a line buffer so we don't need 150 KiB of RAM.
	static uint16_t line_buffer[PIC_WIDTH];

	flash25q64_init();

	// Optional sanity check: 25Q64JVSIQ JEDEC ID is 0xEF4017.
	// (void)flash25q64_read_id();

	flash25q64_chip_erase();

	for (uint16_t y = 0; y < PIC_HEIGHT; y++) {
		const uint8_t* row = picture + (uint32_t)y * PIC_WIDTH;
		for (uint16_t x = 0; x < PIC_WIDTH; x++) {
			line_buffer[x] = palette[row[x]];
		}
		uint32_t addr = FLASH_PIC_ADDR + (uint32_t)y * (PIC_WIDTH * 2);
		flash25q64_program(addr, (const uint8_t*)line_buffer, PIC_WIDTH * 2);
	}
}

static void display_picture_from_flash(void) {
	static uint16_t line_buffer[PIC_WIDTH];

	display1_setWindow(0, 0, PIC_WIDTH, PIC_HEIGHT);

	for (uint16_t y = 0; y < PIC_HEIGHT; y++) {
		uint32_t addr = FLASH_PIC_ADDR + (uint32_t)y * (PIC_WIDTH * 2);
		flash25q64_read(addr, (uint8_t*)line_buffer, PIC_WIDTH * 2);
		display1_sendBytesDMA(line_buffer, PIC_WIDTH);
	}

	display1_waitDMA();
}

int main(void) {
	SysTick_Config(SystemCoreClock / 1000); // 1ms tick

	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	LED_INIT();
	BUTT_INIT();

	display1_init(0);

	decode_and_store_picture();
	display_picture_from_flash();

	while (1) {
		delay_ms(32);
		if (BUTT_READ()) ledOn(); else ledOff();
	}
}
