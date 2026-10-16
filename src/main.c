#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "ili9341/display.h"
#include "ov7670/camera.h"

// Splash image: 256-entry RGB565 palette followed by 240x320 indexed pixels.
extern const uint8_t picData[];
static const uint16_t* palette = (const uint16_t*) picData;
static const uint8_t* picture = picData + 512;

static uint16_t gfxPalette[256];
static uint8_t frame[CAMERA_W * CAMERA_H];   // RGB332 straight from the sensor
static uint16_t gfx16[DIGIT_W * DIGIT_H];    // scratch for display1_number

// RGB332 -> RGB565, spreading the few bits over the full range
static uint16_t hydrate(uint8_t rgb332) {
	uint16_t r = (rgb332 >> 5) & 0x07;
	uint16_t g = (rgb332 >> 2) & 0x07;
	uint16_t b =  rgb332       & 0x03;

	uint16_t r5 = (r << 2) | (r >> 1);
	uint16_t g6 = (g << 3) | (g << 0);
	uint16_t b5 = (b << 3) | (b << 1) | (b >> 1);

	return (r5 << 11) | (g6 << 5) | b5;
}

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

// PC13 is active low
static void ledOn(void)  { LED_LOW(); }
static void ledOff(void) { LED_HIGH(); }

// Camera init failed: show the status and the ID bytes, blink `status` times.
static void halt_camera_error(camera_status_t status) {
	display1_clear(0x00, 0, 0, 240, 320);
	display1_number(gfx16, (uint16_t) status, 120, 140);   // 1 = no SCCB answer, 2 = wrong ID, 3 = register write failed
	display1_number(gfx16, camera_id(), 120, 160);         // PID<<8|VER as read: 30323 (0x7673) is an OV7670
	while (1) {
		for (int i = 0; i < (int) status; i++) {
			ledOn();  delay_ms(150);
			ledOff(); delay_ms(150);
		}
		delay_ms(800);
	}
}

int main(void) {
	clock_init_84mhz();
	SysTick_Config(SystemCoreClock / 1000); // 1 ms tick

	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	for (uint16_t i = 0; i < 256; ++i) gfxPalette[i] = hydrate((uint8_t) i);

	LED_INIT();
	ledOff();
	BUTT_INIT();

	display1_init(0);
	display1_sendBytesIndexed(picture, palette, 240 * 320);

	camera_status_t status = camera_init();
	if (status != CAMERA_OK) halt_camera_error(status);

	// The sensor delivers 320x240 line by line; turn the panel to match so a
	// frame streams straight into it.
	display1_madctl(DISPLAY1_LANDSCAPE);

	bool test_pattern = false;
	bool was_pressed = false;
	while (1) {
		// KEY toggles the sensor color bars: checks wiring without optics
		bool pressed = !BUTT_READ();
		if (pressed && !was_pressed) {
			test_pattern = !test_pattern;
			camera_test_pattern(test_pattern);
		}
		was_pressed = pressed;

		if (camera_capture(frame) > 0) {
			ledOn();
			display1_setWindow(0, 0, CAMERA_W, CAMERA_H);
			display1_sendBytesIndexed(frame, gfxPalette, CAMERA_W * CAMERA_H);
			ledOff();
		} else {
			// No frame: sensor not clocking or sync lost. Keep trying.
			delay_ms(100);
		}
	}
}
