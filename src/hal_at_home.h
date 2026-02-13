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

#define MODER(port, pin, mode)      BIT_ASSIGN(port->MODER  , pin, 2, mode)
// pins 0-7 only
#define AFR(port, pin, range, mode) BIT_ASSIGN(port->AFR[range], pin, 4, mode) // todo range
#define OSPEEDR(port, pin, mode)    BIT_ASSIGN(port->OSPEEDR, pin, 2, mode)
#define OTYPER(port, pin, mode)     BIT_ASSIGN(port->OTYPER,  pin, 1, mode)

#define GPIO_PIN(v)  ((uint16_t)(1 << v))

#define SPI_TXE_READY(spi)  (((spi)->SR & SPI_SR_TXE)  != 0)
#define SPI_RXNE_READY(spi) (((spi)->SR & SPI_SR_RXNE) != 0)
#define SPI_BSY(spi)        (((spi)->SR & SPI_SR_BSY)  != 0)

#define I2C_START(i2c) \
	(i2c)->CR1 |= I2C_CR1_START; \
	while(!((i2c)->SR1 & I2C_SR1_SB));
#define I2C_STOP(i2c) \
	(i2c)->CR1 |= I2C_CR1_STOP; \
	while((i2c)->SR2 & I2C_SR2_BUSY);
#define I2C_ADDRESS(i2c, address) \
	(i2c)->DR = (address); \
	while (!((i2c)->SR1 & I2C_SR1_ADDR));
#define I2C_FLUSH(i2c) \
	(void)(i2c)->SR1; \
	(void)(i2c)->SR2;
#define I2C_SEND(i2c, payload) \
	(i2c)->DR = (payload); \
	while (!((i2c)->SR1 & I2C_SR1_TXE));
#define I2C_READ(i2c, dst) \
	while (!((i2c)->SR1 & I2C_SR1_RXNE)); \
	uint8_t (dst) = (i2c)->DR;
#define I2C_ACK_IN(i2c) \
	(i2c)->CR1 |= I2C_CR1_ACK;
#define I2C_ACK_OUT(i2c) \
	(i2c)->CR1 &= ~I2C_CR1_ACK;

void delay_ms(uint32_t ms);
