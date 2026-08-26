#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "tusb.h"
#include "usb/board.h"
#include "usb/xgip_host.h"
#include "sh1106/display.h"
#include <stdlib.h>
#include <math.h>

#define FILL(v, d) for (int i = 0; i < (sizeof(v) / sizeof(v[0])); ++i) { v[i] = d; }
#define AT(x, y, w) ((x) + (y) * (w))

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

uint8_t gfx[1024];

void ledOff() {
	LED_HIGH();
}
void ledOn() {
	LED_LOW();
}

// Hard fault indicator: LED on solid
void HardFault_Handler(void) {
	// Ensure GPIOC is enabled and PC13 is output
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
	GPIOC->MODER |= (1U << (13 * 2));
	ledOn();
	while (1) __NOP();
}

// Simple busy-wait delay using CPU cycles.
// Must be used BEFORE SysTick is configured in board_init().
static void delay_ms_blocking(uint32_t ms) {
	// At 16 MHz HSI this is roughly 16k cycles/ms; at 84 MHz roughly 84k cycles/ms.
	// Using a conservative value that works at both speeds.
	for (uint32_t i = 0; i < ms; i++) {
		for (volatile uint32_t d = 0; d < 4000; d++) __NOP();
	}
}

static void blink(uint32_t count, uint32_t duration) {
	for (int i = 0; i < count * 2; i++) {
		if (i & 1) ledOn(); else ledOff();
		delay_ms_blocking(duration);
	}
	ledOff();
}

//--------------------------------------------------------------------+
// USB host event callbacks -> LED diagnostics
//
// LED states (PC13, active low):
//   slow blink (400 ms)      : no device connected
//   fast blink (80 ms)       : device attached, enumerating
//   medium blink (250 ms)    : device mounted, GIP not powered yet
//   solid ON                 : GIP pad ready, no button pressed
//   OFF                      : GIP pad ready AND button pressed
//--------------------------------------------------------------------+

static volatile bool usb_dev_mounted = false;

void tuh_mount_cb(uint8_t daddr) {
	(void) daddr;
	usb_dev_mounted = true;
}

void tuh_umount_cb(uint8_t daddr) {
	(void) daddr;
	usb_dev_mounted = false;
}

int main(void) {
	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	LED_INIT();
	BUTT_INIT();
	ledOff();

	// Initialize board (clocks, GPIO, USB pins, SysTick)
	board_init(); // <- clocks get init into 84mhz there
	SysTick_Config(SystemCoreClock / 1000); // 1ms tick
	display0_init();
	FILL(gfx, 0x00);
	display0_updateTranslated(gfx);

	// board_init() passed: two quick blinks
	blink(2, 300);

	// Initialize TinyUSB host stack on roothub port 0 (OTG_FS)
	tusb_rhport_init_t host_init = {.role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_FULL};
	tusb_init(BOARD_TUH_RHPORT, &host_init);

	// Post-TinyUSB init (if needed)
	board_init_after_tusb();

	// All init passed: LED off, then enter normal loop
	ledOff();

	uint32_t status[7] = { 771, 7001, 7420, 777, 7, 17, 27 };

	while (1) {
		// TinyUSB host task
		tuh_task();

		// GIP power-on / keep-alive packets
		xgip_task();

		// LED diagnostics:
		//  - pad ready: solid ON, OFF while a button is pressed
		//  - pad mounted but not powered: medium blink
		//  - device attached/enumerating: fast blink
		//  - nothing connected: slow blink
		static uint32_t last_blink = 0;
		static bool led_state = false;
		uint32_t now = tusb_time_millis_api();

		// if (xgip_mounted()) {
		// 	if (xgip_any_button_pressed()) {
		// 		ledOff();
		// 	} else {
		// 		ledOn();
		// 	}
		// } else {
		// 	uint32_t period = usb_dev_mounted ? 250 :
		// 			tuh_connected(1) ? 80 : 400;
		// 	if (now - last_blink >= period) {
		// 		last_blink = now;
		// 		led_state = !led_state;
		// 		if (led_state) ledOn(); else ledOff();
		// 	}
		// }

		display0_updateNumbers(status, 7);
		// display0_update_48_32(gfx, USB_OTG_FS->GCCFG, USB_OTG_FS->GINTSTS);
		delay_ms(10);
	}
}
