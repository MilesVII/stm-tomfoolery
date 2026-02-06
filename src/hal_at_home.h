#pragma once

#include "stm32f411xe.h"
#include <stdint.h>

#define PIN_SET(port, pin) ((port)->BSRR = (pin))
#define PIN_CLR(port, pin) ((port)->BSRR = ((pin) << 16))

#define _POW2_1 2
#define _POW2_2 4
#define _POW2_3 8
#define _POW2_4 16
#define _POW2_5 32
#define _POW2(v) _POW2_##v
#define BIT_ASSIGN(l, offset, size, value)\
	l &= ~((_POW2(size) - 1) << (offset * size)); \
	l |=  (            value << (offset * size));

#define MODER(port, pin, mode)   BIT_ASSIGN(port->MODER  , pin, 2, mode)
#define AFR(port, pin, mode)     BIT_ASSIGN(port->AFR[0] , pin, 4, mode)
#define OSPEEDR(port, pin, mode) BIT_ASSIGN(port->OSPEEDR, pin, 2, mode)

#define GPIO_PIN(v)  ((uint16_t)(1 << v))

#define SPI_TXE_READY(spi)  (((spi)->SR & SPI_SR_TXE)  != 0)
#define SPI_RXNE_READY(spi) (((spi)->SR & SPI_SR_RXNE) != 0)
#define SPI_BSY(spi)        (((spi)->SR & SPI_SR_BSY)  != 0)

void delay_ms(uint32_t ms);
