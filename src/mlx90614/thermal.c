#include "stm32f411xe.h"
#include "../hal_at_home.h"
#include "thermal.h"

#define I2C_THERMAL_ADDRESS 0x5A
#define I2C_THERMAL_OBJECT  0x07
#define I2C_THERMAL_AMBIENT 0x06

#define I2C I2C1
DECLARE_I2C(SCL, B, 6, 4);
DECLARE_I2C(SDA, B, 7, 4);


void thermal_init() {
	RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

	SCL_INIT();
	SDA_INIT();

	I2C->CR1 = I2C_CR1_SWRST;
	I2C->CR1 = 0;

	I2C->CR2 |= 0x10;
	I2C->CCR |= 0x50;
	I2C->TRISE |= (11 << 0);
	I2C->CR1 |= I2C_CR1_PE;
}

uint16_t thermal_poll() {
	I2C_START(I2C);

	I2C_ADDRESS_W(I2C, I2C_THERMAL_ADDRESS);
	I2C_SEND(I2C, I2C_THERMAL_OBJECT);
	// I2C_ACK_IN(I2C);

	I2C_START(I2C);
	// I2C_FLUSH(I2C);
	I2C_ADDRESS_R(I2C, I2C_THERMAL_ADDRESS);

	uint8_t low;
	uint8_t high;
	uint8_t pec;
	I2C_ACK_IN(I2C);
	I2C_READ(I2C, low);
	I2C_ACK_IN(I2C);
	I2C_READ(I2C, high);
	I2C_ACK_OUT(I2C);
	I2C_READ(I2C, pec);

	I2C_STOP(I2C);

	return (high << 8) | low;
}