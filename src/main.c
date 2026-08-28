#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "tusb.h"
#include "usb/board.h"
#include "usb/xgip_host.h"
#include "sh1106/display.h"
#include "tetris.h"
#include <stdlib.h>
#include <math.h>

#define FILL(v, d) for (int i = 0; i < (sizeof(v) / sizeof(v[0])); ++i) { v[i] = d; }
#define AT(x, y, w) ((x) + (y) * (w))

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);

#define GPAD_CROSS_U 1
#define GPAD_CROSS_D 2
#define GPAD_CROSS_L 4
#define GPAD_CROSS_R 8
#define GPAD_LEVER_L 16
#define GPAD_LEVER_R 32
#define GPAD_LEVER_A (GPAD_LEVER_L | GPAD_LEVER_R)
#define GPAD_CNTRL_M 1024 // menu
#define GPAD_CNTRL_W 2048 // win
#define GPAD_BUTTS_A 4096
#define GPAD_BUTTS_B 8196
#define GPAD_BUTTS_X 16384
#define GPAD_BUTTS_Y 32768

uint8_t gfx[1024];

void ledOff() {
	LED_HIGH();
}
void ledOn() {
	LED_LOW();
}
#define TARGET_FPS 60.0
const float targetFrameTimeMS = 1000.0 / TARGET_FPS;
#define DT() (float)DWT->CYCCNT * 1000.0 / SystemCoreClock
#define DTC() DWT->CYCCNT
#define DT_RESET() DWT->CYCCNT = 0

// screen size
#define SW 240
#define SH 320

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

#define OTG_FS_HPRT (*(volatile uint32_t *)(USB_OTG_FS_PERIPH_BASE + 0x440UL))

int main(void) {
	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	LED_INIT();
	BUTT_INIT();
	ledOff();

	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	srand(0);

	// Initialize board (clocks, GPIO, USB pins, SysTick)
	board_init(); // <- clocks get init into 84mhz there
	USB_OTG_FS->GUSBCFG |= USB_OTG_GUSBCFG_FHMOD;
	SysTick_Config(SystemCoreClock / 1000); // 1ms tick

	// Initialize TinyUSB host stack on roothub port 0 (OTG_FS)
	tusb_rhport_init_t host_init = {.role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_FULL};
	tusb_init(BOARD_TUH_RHPORT, &host_init);

	// Post-TinyUSB init (if needed)
	board_init_after_tusb();

	display0_init();
	display0_updateTranslated(gfx);
	display0_clear();
	tetris_init(gfx);
	ledOff();

	uint16_t gameIO;
	uint16_t score = TETRIS_SCORE_DIRTY;
	float smolMS = 0.0;
	float fullMS = 0.0;

	while (1) {
		DT_RESET();
		tuh_task();
		xgip_task();

		uint32_t padInput = xgip_buttons();
		// xgip_mounted();
		gameIO =
			(gameIO << 8) |
			(padInput & (GPAD_LEVER_R | GPAD_LEVER_L) ? TETRIS_IO_D : 0x00) |
			(padInput & GPAD_BUTTS_Y ? TETRIS_IO_S  : 0x00) |
			(padInput & GPAD_CROSS_L ? TETRIS_IO_L  : 0x00) |
			(padInput & GPAD_CROSS_R ? TETRIS_IO_R  : 0x00) |
			(padInput & GPAD_CNTRL_M ? TETRIS_IO_P  : 0x00) |
			(padInput & GPAD_BUTTS_A ? TETRIS_IO_FD : 0x00);

		tetris_update(gfx, gameIO, fullMS, &score);
		display0_updateTranslated(gfx);
		if (score & TETRIS_SCORE_DIRTY) {
			score = score & ~TETRIS_SCORE_DIRTY;
		}

		smolMS = DT();
		if (smolMS < targetFrameTimeMS) {
			float sleepTimeMS = roundf(targetFrameTimeMS - smolMS);
			if (sleepTimeMS > 0) delay_ms((uint32_t)sleepTimeMS);
		} else {
			ledOn();
		}
		fullMS = DT();
	}
}
