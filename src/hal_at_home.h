#pragma once

#include "stm32f411xe.h"
#include <stdint.h>

#define PIN_SET(port, pin)   ((port)->BSRR = (pin))
#define PIN_CLR(port, pin)   ((port)->BSRR = ((pin) << 16))

#define MODER(PORT, PIN, MODE) \
	PORT->MODER &= ~(   3 << (PIN * 2)); \
	PORT->MODER |=  (MODE << (PIN * 2));

#define AFR(PORT, PIN, MODE) \
	PORT->AFR[0] &= ~( 0xF << (PIN * 4)); \
	PORT->AFR[0] |=  (MODE << (PIN * 4));

#define OSPEEDR(PORT, PIN, MODE) \
	PORT->OSPEEDR &= ~(   3 << (PIN * 2)); \
	PORT->OSPEEDR |=  (MODE << (PIN * 2));

#define GPIO_PIN_0   ((uint16_t)0x0001)
#define GPIO_PIN_1   ((uint16_t)0x0002)
#define GPIO_PIN_2   ((uint16_t)0x0004)
#define GPIO_PIN_3   ((uint16_t)0x0008)
#define GPIO_PIN_4   ((uint16_t)0x0010)
#define GPIO_PIN_5   ((uint16_t)0x0020)
#define GPIO_PIN_6   ((uint16_t)0x0040)
#define GPIO_PIN_7   ((uint16_t)0x0080)
#define GPIO_PIN_8   ((uint16_t)0x0100)
#define GPIO_PIN_9   ((uint16_t)0x0200)
#define GPIO_PIN_10  ((uint16_t)0x0400)
#define GPIO_PIN_11  ((uint16_t)0x0800)
#define GPIO_PIN_12  ((uint16_t)0x1000)
#define GPIO_PIN_13  ((uint16_t)0x2000)
#define GPIO_PIN_14  ((uint16_t)0x4000)
#define GPIO_PIN_15  ((uint16_t)0x8000)
#define GPIO_PIN_All ((uint16_t)0xFFFF)

void delay_ms(uint32_t ms);
