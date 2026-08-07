#include "board.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// SysTick (1ms tick) - uses existing handler from hal_at_home.c
//--------------------------------------------------------------------+

extern volatile uint32_t ticks;

uint32_t tusb_time_millis_api(void) {
    return ticks;
}

//--------------------------------------------------------------------+
// Clock initialization
//--------------------------------------------------------------------+

static void clock_init(void) {
    // Set flash latency for 84 MHz (2 wait states) before switching clocks
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_2WS;

    // Try HSE first (25 MHz crystal on blackpill)
    RCC->CR |= RCC_CR_HSEON;
    uint32_t timeout = 0x100000;
    while (!(RCC->CR & RCC_CR_HSERDY) && --timeout) {}

    if (timeout != 0) {
        // HSE is ready: configure PLL from HSE
        // PLL_VCO = 25/25 * 336 = 336 MHz
        // SYSCLK = 336 / 4 = 84 MHz
        // USB = 336 / 7 = 48 MHz
        RCC->PLLCFGR = (PLL_Q << 24) | (PLL_P << 17) | (PLL_N << 6) | (PLL_M << 0) | RCC_PLLCFGR_PLLSRC_HSE;
    } else {
        // HSE failed to start: fall back to HSI (16 MHz internal RC)
        // Make sure HSE is off so it does not interfere
        RCC->CR &= ~RCC_CR_HSEON;

        // Wait for HSI ready (should already be ready)
        timeout = 0x100000;
        while (!(RCC->CR & RCC_CR_HSIRDY) && --timeout) {}

        // PLL from HSI: 16/16 * 336 / 4 = 84 MHz, USB = 48 MHz
        RCC->PLLCFGR = (7U << 24) | (PLL_P << 17) | (336U << 6) | (16U << 0) | RCC_PLLCFGR_PLLSRC_HSI;
    }

    RCC->CR |= RCC_CR_PLLON;
    timeout = 0x100000;
    while (!(RCC->CR & RCC_CR_PLLRDY) && --timeout) {}
    if (timeout == 0) {
        // PLL failed to lock: turn LED on solid and halt
        board_led_write(true);
        while (1) __NOP();
    }

    // Set bus prescalers
    // AHB = SYSCLK / 1 = 84 MHz
    // APB1 = AHB / 2 = 42 MHz (must be <= 42 MHz for USB)
    // APB2 = AHB / 1 = 84 MHz
    RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_PPRE1_DIV2;

    // Switch to PLL
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    timeout = 0x100000;
    while (((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) && --timeout) {}
    if (timeout == 0) {
        // Switch to PLL failed: turn LED on solid and halt
        board_led_write(true);
        while (1) __NOP();
    }

    // Update SystemCoreClock variable
    SystemCoreClock = 84000000UL;
}

//--------------------------------------------------------------------+
// GPIO initialization
//--------------------------------------------------------------------+

static void gpio_init(void) {
    // Enable GPIO clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

    // LED (PC13) - output, push-pull
    GPIOC->MODER &= ~(3U << (13 * 2));
    GPIOC->MODER |= (1U << (13 * 2));
    GPIOC->OTYPER &= ~(1U << 13);
    GPIOC->OSPEEDR |= (3U << (13 * 2));
    GPIOC->PUPDR &= ~(3U << (13 * 2));

    // Turn LED off (PC13 is active low on blackpill)
    GPIOC->BSRR = (1U << 13);

    // Button (PA0) - input, pull-up
    GPIOA->MODER &= ~(3U << (0 * 2));
    GPIOA->PUPDR &= ~(3U << (0 * 2));
    GPIOA->PUPDR |= (1U << (0 * 2));
}

//--------------------------------------------------------------------+
// USB pin initialization (OTG_FS)
//--------------------------------------------------------------------+

static void usb_pin_init(void) {
    // PA11 (DM), PA12 (DP) - alternate function 10 (OTG_FS)
    GPIOA->MODER &= ~((3U << 11 * 2) | (3U << 12 * 2));
    GPIOA->MODER |= ((2U << 11 * 2) | (2U << 12 * 2));
    GPIOA->AFR[1] &= ~((0xF << (11 - 8) * 4) | (0xF << (12 - 8) * 4));
    GPIOA->AFR[1] |= ((10U << (11 - 8) * 4) | (10U << (12 - 8) * 4));
    GPIOA->OSPEEDR |= ((3U << 11 * 2) | (3U << 12 * 2));
    GPIOA->PUPDR &= ~((3U << 11 * 2) | (3U << 12 * 2));

    // PA9 (VBUS) - input, no pull (sensed by hardware)
    GPIOA->MODER &= ~(3U << (9 * 2));
    GPIOA->PUPDR &= ~(3U << (9 * 2));

    // PA10 (ID) - alternate function 10 (OTG_FS), open-drain, pull-up
    GPIOA->MODER &= ~(3U << (10 * 2));
    GPIOA->MODER |= (2U << (10 * 2));
    GPIOA->AFR[1] &= ~(0xF << (10 - 8) * 4);
    GPIOA->AFR[1] |= (10U << (10 - 8) * 4);
    GPIOA->OTYPER |= (1U << 10);
    GPIOA->PUPDR &= ~(3U << (10 * 2));
    GPIOA->PUPDR |= (1U << (10 * 2));

    // PA8 (SOF) - alternate function 10 (OTG_FS)
    GPIOA->MODER &= ~(3U << (8 * 2));
    GPIOA->MODER |= (2U << (8 * 2));
    GPIOA->AFR[1] &= ~(0xF << (8 - 8) * 4);
    GPIOA->AFR[1] |= (10U << (8 - 8) * 4);

    // Enable USB OTG FS clock
    RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
}

//--------------------------------------------------------------------+
// Board API implementation
//--------------------------------------------------------------------+

void board_init(void) {
    // Initialize GPIO first so the LED can show clock/HSE failure states
    gpio_init();

    // Diagnostic: LED on briefly to show gpio_init passed
    board_led_write(true);
    for (volatile uint32_t d = 0; d < 200000; d++) __NOP();
    board_led_write(false);

    clock_init();

    // Diagnostic: LED on briefly to show clock_init passed
    board_led_write(true);
    for (volatile uint32_t d = 0; d < 200000; d++) __NOP();
    board_led_write(false);

    usb_pin_init();

    // Diagnostic: LED on briefly to show usb_pin_init passed
    board_led_write(true);
    for (volatile uint32_t d = 0; d < 200000; d++) __NOP();
    board_led_write(false);

    // Configure SysTick for 1ms (84 MHz / 1000 = 84000)
    SysTick_Config(SystemCoreClock / 1000);

    // Diagnostic: LED on briefly to show SysTick configured
    board_led_write(true);
    for (volatile uint32_t d = 0; d < 200000; d++) __NOP();
    board_led_write(false);

    // Enable OTG_FS interrupt
    NVIC_SetPriority(OTG_FS_IRQn, 2);
    NVIC_EnableIRQ(OTG_FS_IRQn);
}

void board_init_after_tusb(void) {
    // Nothing extra needed
}

void board_led_write(bool state) {
    if (state) {
        // LED on: drive low (active low)
        GPIOC->BSRR = (1U << (13 + 16));
    } else {
        // LED off: drive high
        GPIOC->BSRR = (1U << 13);
    }
}

uint32_t board_button_read(void) {
    // Active low: returns 0 when pressed, 1 when released
    return (GPIOA->IDR & BUTTON_PIN) ? 1 : 0;
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
    // STM32F411 unique ID is at 0x1FFF7A10 (96 bits = 12 bytes)
    const uint32_t *uid = (const uint32_t *)0x1FFF7A10;
    size_t len = 12; // 96 bits

    if (len > max_len) {
        len = max_len;
    }

    for (size_t i = 0; i < len; i++) {
        id[i] = (uint8_t)(uid[i / 4] >> ((i % 4) * 8));
    }

    return len;
}

//--------------------------------------------------------------------+
// USB interrupt handler
//--------------------------------------------------------------------+

void OTG_FS_IRQHandler(void) {
    tusb_int_handler(0, true);
}
