#ifndef BOARD_H_
#define BOARD_H_

#include "stm32f411xe.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

//--------------------------------------------------------------------+
// Pin definitions (register-level, no HAL)
//--------------------------------------------------------------------+

#define LED_PIN                 (1U << 13)
#define LED_PORT                GPIOC
#define BUTTON_PIN              (1U << 0)
#define BUTTON_PORT             GPIOA
#define BUTTON_ACTIVE_LOW       1

// USB pins (OTG_FS)
#define USB_DM_PIN              GPIO_PIN_11
#define USB_DP_PIN              GPIO_PIN_12
#define USB_ID_PIN              GPIO_PIN_10
#define USB_VBUS_PIN            GPIO_PIN_9
#define USB_SOF_PIN             GPIO_PIN_8
#define USB_PORT                GPIOA

//--------------------------------------------------------------------+
// Clock configuration
//--------------------------------------------------------------------+

// HSE = 25 MHz (blackpill board)
// PLL: PLLM=25, PLLN=336, PLLP=/4, PLLQ=7
// SYSCLK = 25/25 * 336 / 4 = 84 MHz
// USB clock = 25/25 * 336 / 7 = 48 MHz
// Note: PLLP register encoding is 0=/2, 1=/4, 2=/6, 3=/8
#define HSE_VALUE               25000000UL
#define PLL_M                   25
#define PLL_N                   336
#define PLL_P                   1   // /4
#define PLL_Q                   7

//--------------------------------------------------------------------+
// Board API (minimal, no HAL)
//--------------------------------------------------------------------+

void board_init(void);
void board_init_after_tusb(void);
void board_led_write(bool state);
uint32_t board_button_read(void);
size_t board_get_unique_id(uint8_t id[], size_t max_len);

// TinyUSB time base (SysTick 1ms)
uint32_t tusb_time_millis_api(void);

#endif /* BOARD_H_ */
