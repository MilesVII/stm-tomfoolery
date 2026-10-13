#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "board.h"
#include "tusb.h"

uint32_t tusb_time_millis_api(void) {
	return ticks;
}

// Something fundamental failed: LED (PC13, active low) on solid, forever.
static void halt_with_led(void) {
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
	MODER(GPIOC, 13, 1);
	PIN_CLR(GPIOC, GPIO_PIN(13));
	while (1) __NOP();
}

//--------------------------------------------------------------------+
// Clocks
//--------------------------------------------------------------------+

static void clock_init(void) {
	// 84 MHz at 3.3 V needs 2 wait states; also turn on prefetch and caches.
	FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_2WS;

	// Prefer the 25 MHz crystal; fall back to the 16 MHz HSI if it never starts
	// (USB tolerates the HSI poorly, but at least the LED can tell the story).
	RCC->CR |= RCC_CR_HSEON;
	uint32_t timeout = 0x100000;
	while (!(RCC->CR & RCC_CR_HSERDY) && --timeout) {}

	uint32_t pll_src, pll_m;
	if (timeout) {
		pll_src = RCC_PLLCFGR_PLLSRC_HSE;
		pll_m = 25;
	} else {
		RCC->CR &= ~RCC_CR_HSEON;
		pll_src = RCC_PLLCFGR_PLLSRC_HSI;
		pll_m = 16;
	}

	// VCO = (src / M) * N = 1 MHz * 336 = 336 MHz
	// SYSCLK = VCO / P = 336 / 4 = 84 MHz      (PLLP field: 0=/2 1=/4 2=/6 3=/8)
	// USB    = VCO / Q = 336 / 7 = 48 MHz
	RCC->PLLCFGR =
		(7u << RCC_PLLCFGR_PLLQ_Pos) |
		(1u << RCC_PLLCFGR_PLLP_Pos) |
		(336u << RCC_PLLCFGR_PLLN_Pos) |
		(pll_m << RCC_PLLCFGR_PLLM_Pos) |
		pll_src;

	RCC->CR |= RCC_CR_PLLON;
	timeout = 0x100000;
	while (!(RCC->CR & RCC_CR_PLLRDY) && --timeout) {}
	if (!timeout) halt_with_led();

	// AHB = 84 MHz, APB2 = 84 MHz, APB1 = 42 MHz (APB1 max is 50 MHz)
	RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_PPRE1_DIV2;

	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
	timeout = 0x100000;
	while (((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) && --timeout) {}
	if (!timeout) halt_with_led();

	SystemCoreClock = 84000000u;
}

//--------------------------------------------------------------------+
// USB OTG FS
//--------------------------------------------------------------------+

static void usb_init(void) {
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

	// PA11 D-, PA12 D+: AF10, very high speed, no pull.
	// VBUS sensing is off in TinyUSB (CFG_TUD_VBUS_DETECT_HW = 0) and this is a
	// device-only setup, so PA8 (SOF), PA9 (VBUS) and PA10 (ID) stay free.
	for (uint32_t pin = 11; pin <= 12; pin++) {
		MODER(GPIOA, pin, 2);
		AFR(GPIOA, pin, 10);
		OSPEEDR(GPIOA, pin, 3);
		PUPDR(GPIOA, pin, 0);
	}

	RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;

	NVIC_SetPriority(OTG_FS_IRQn, 2);
	NVIC_EnableIRQ(OTG_FS_IRQn);
}

void OTG_FS_IRQHandler(void) {
	tusb_int_handler(0, true);
}

//--------------------------------------------------------------------+
// Board API
//--------------------------------------------------------------------+

void board_init(void) {
	clock_init();
	SysTick_Config(SystemCoreClock / 1000);
	usb_init();
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
	// 96-bit unique device ID register block
	const uint8_t* uid = (const uint8_t*)UID_BASE;
	size_t len = 12;
	if (len > max_len) len = max_len;
	for (size_t i = 0; i < len; i++) id[i] = uid[i];
	return len;
}
