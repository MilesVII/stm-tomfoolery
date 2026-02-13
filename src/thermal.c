#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "thermal.h"

#define I2C_THERMAL_ADDRESS 0x5A
#define I2C_THERMAL_OBJECT  0x07
#define I2C_THERMAL_AMBIENT 0x06

// AF04
#define SCL_PORT    GPIOB
#define SCL_PIN     6

#define SDA_PORT    GPIOB
#define SDA_PIN     7

#define I2C_PIN_INIT(pin, range) \
	MODER(pin##_PORT, pin##_PIN, 2); \
	AFR(pin##_PORT, pin##_PIN, range, 4); \
	OSPEEDR(pin##_PORT, pin##_PIN, 3) \
	OTYPER(pin##_PORT, pin##_PIN, 1);

void thermal_initI2C() {
	RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

	I2C_PIN_INIT(SCL, 0);
	I2C_PIN_INIT(SDA, 0);

	I2C1->CR1 = I2C_CR1_SWRST;
	I2C1->CR1 = 0;

	I2C1->CR2 |= 0x10;
	I2C1->CCR |= 0x50;
	I2C1->TRISE |= (11 << 0);
	I2C1->CR1 |= I2C_CR1_PE;
}

uint16_t thermal_poll() {
	I2C_START(I2C1);
	I2C_FLUSH(I2C1);
	I2C_ADDRESS(I2C1, I2C_THERMAL_ADDRESS << 1);
	I2C_FLUSH(I2C1);

	I2C_SEND(I2C1, I2C_THERMAL_OBJECT);

	I2C_ACK_IN(I2C1);

	I2C_START(I2C1);
	I2C_FLUSH(I2C1);
	I2C_ADDRESS(I2C1, (I2C_THERMAL_ADDRESS << 1) | 0x01);
	I2C_FLUSH(I2C1);

	I2C_READ(I2C1, low);
	I2C_READ(I2C1, high);
	I2C_ACK_IN(I2C1);
	I2C_STOP(I2C1);
	I2C_READ(I2C1, pec);

	return (high << 8) | low;
}
