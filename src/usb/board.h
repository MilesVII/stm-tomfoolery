#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Blackpill (STM32F411CEU6, 25 MHz HSE) bring-up for TinyUSB:
//   84 MHz SYSCLK from the PLL (HSI fallback), 48 MHz USB clock,
//   1 ms SysTick, PA11/PA12 as OTG_FS D-/D+, OTG_FS interrupt.
void board_init(void);

// 96-bit device unique ID, up to `max_len` bytes copied; returns the count.
size_t board_get_unique_id(uint8_t id[], size_t max_len);

// Time base for TinyUSB (SysTick, 1 ms).
uint32_t tusb_time_millis_api(void);
