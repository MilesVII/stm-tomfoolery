#include "stm32f411xe.h"
#include "hal_at_home.h"
#include <sys/stat.h>
#include <errno.h>

// stub syscalls
int _write(int file, char *ptr, int len) { return len; }
int _read(int file, char *ptr, int len) { return 0; }
int _close(int file) { return -1; }
int _fstat(int file, struct stat *st) { st->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { return 1; }
int _lseek(int file, int ptr, int dir) { return 0; }
void* _sbrk(int incr) {
	extern char _end; // from linker script
	static char *heap_end;
	char *prev;

	if (heap_end == 0)
		heap_end = &_end;

	prev = heap_end;
	heap_end += incr;
	return prev;
}

// sleep
volatile uint32_t ticks;

void SysTick_Handler(void) {
	ticks++;
}

void delay_ms(uint32_t ms) {
	uint32_t start = ticks;
	while ((ticks - start) < ms) {
		__NOP();
	};
}
// clocks
static void clock_halt(void) {
	// LED on PC13 (active low) solid, forever
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
	MODER(GPIOC, 13, 1);
	PIN_CLR(GPIOC, GPIO_PIN(13));
	while (1) __NOP();
}

void clock_init_84mhz(void) {
	// 84 MHz at 3.3 V needs 2 wait states; also turn on prefetch and caches.
	FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_2WS;

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
	if (!timeout) clock_halt();

	// AHB = 84 MHz, APB2 = 84 MHz, APB1 = 42 MHz (APB1 max is 50 MHz)
	RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_PPRE1_DIV2;

	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
	timeout = 0x100000;
	while (((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) && --timeout) {}
	if (!timeout) clock_halt();

	SystemCoreClock = 84000000u;
}
