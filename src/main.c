#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "flash/flash25q64.h"
#include "msc_disk.h"
#include "usb/board.h"
#include "tusb.h"

// Hold KEY (PA0, active low) this long to erase the whole flash chip.
#define ERASE_HOLD_MS 2000u

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

// PC13 is active low on the blackpill
static void ledOn(void)  { LED_LOW(); }
static void ledOff(void) { LED_HIGH(); }

// Hard fault indicator: LED on solid
void HardFault_Handler(void) {
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
	MODER(GPIOC, 13, 1);
	ledOn();
	while (1) __NOP();
}

// Flash chip missing or not answering: two quick blinks, pause, forever.
static void halt_no_flash(void) {
	while (1) {
		for (int i = 0; i < 2; i++) {
			ledOn();  delay_ms(100);
			ledOff(); delay_ms(100);
		}
		delay_ms(600);
	}
}

// LED status patterns, in (on_ms, period_ms):
//   chip erase running    100 / 200   (fast flicker)
//   flash write failed     50 / 100   (frantic flicker)
//   mounted by a host      50 / 1000  (heartbeat)
//   plugged in, not mounted 250 / 500
static void led_task(uint32_t now) {
	uint32_t on, period;
	if (msc_disk_erasing())    { on = 100; period = 200; }
	else if (msc_disk_error()) { on = 50;  period = 100; }
	else if (tud_mounted())    { on = 50;  period = 1000; }
	else                       { on = 250; period = 500; }

	if (now % period < on) ledOn(); else ledOff();
}

int main(void) {
	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	LED_INIT();
	ledOff();
	BUTT_INIT();

	board_init(); // 84 MHz, SysTick, USB pins and interrupt

	if (!flash25q64_init()) halt_no_flash();
	msc_disk_init();

	tusb_rhport_init_t dev_init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_FULL };
	tusb_init(BOARD_TUD_RHPORT, &dev_init);

	uint32_t press_start = 0;
	bool erase_armed = true;

	while (1) {
		tud_task();
		msc_disk_task();

		uint32_t now = ticks;

		// Long press on KEY wipes the chip; release re-arms it.
		bool pressed = !BUTT_READ();
		if (!pressed) {
			press_start = now;
			erase_armed = true;
		} else if (erase_armed && now - press_start >= ERASE_HOLD_MS) {
			erase_armed = false;
			msc_disk_chip_erase_start();
		}

		led_task(now);
	}
}
