#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "flash/flash25q64.h"
#include "tusb.h"
#include "usb/board.h"

// Forward declarations for MSC disk cache
void msc_disk_init(void);
void msc_disk_pending_flush(void);

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

static void ledOff() {
	LED_HIGH();
}
static void ledOn() {
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

uint8_t sig[3] = { 0x07, 0xF7, 0x77 };

int main(void) {
	SysTick_Config(SystemCoreClock / 1000);
	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	LED_INIT();

	ledOff();
	// blink(1, 300);

	// Initialize board (clocks, GPIO, USB pins, SysTick)
	board_init();
	// delay_ms_blocking(100);

	// board_init() passed: two quick blinks

	// Initialize flash chip
	flash25q64_init();

	// delay_ms_blocking(100);

	// blink(1, 300);

	// Write a test signature to flash sector 0 (for debugging)
	flash25q64_erase_and_write(0, sig, sizeof(sig));
	// delay_ms_blocking(100);

	uint8_t c[3];
	uint8_t ok = 1;
	flash25q64_read(0, c, 3);
	for (int i = 0; i < 3; ++i) {
		if (c[i] != sig[i]) {
			// blink(1000, 100);
			ok = 0;
			break;
		}
	}
	if (ok) {
		// blink(3, 400);
	} else {
		blink(1000, 100);
	}

	// flash25q64_init() passed: two more quick blinks
	// blink(3, 500);

	// Initialize MSC disk write-back cache
	// msc_disk_init();

	// Initialize TinyUSB device stack on roothub port 0
	// tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_FULL};
	// tusb_init(BOARD_TUD_RHPORT, &dev_init);

	// Post-TinyUSB init (if needed)
	// board_init_after_tusb();

	// All init passed: LED off, then enter normal loop
	// ledOff();

	while (1) {
		// TinyUSB device task
		// tud_task();

		// // Flush MSC write-back cache when safe-removal was requested.
		// // We defer the actual flash erase+write out of the USB callback
		// // so the host doesn't time out during the ~50 ms flash operation.
		// msc_disk_pending_flush();

		// // LED blink: fast when not mounted, slow when mounted
		// static uint32_t last_blink = 0;
		// static bool led_state = false;
		// uint32_t now = tusb_time_millis_api();

		// if (now - last_blink >= (tud_mounted() ? 1000 : 250)) {
		// 	last_blink = now;
		// 	led_state = !led_state;
		// 	if (led_state) {
		// 		ledOn();
		// 	} else {
		// 		ledOff();
		// 	}
		// }
	}
}
