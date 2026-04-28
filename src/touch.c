#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "touch.h"

#define I2C_DEVICE_ADDRESS 0x38
// 0x00 Device mode
// mode
#define I2C_REG_MOD 0xA4
// touch point count
#define I2C_REG_TPC 0x02
// XH XL YH YL
#define I2C_REG_T0_START 0x03
// XH XL YH YL
#define I2C_REG_T1_START 0x09

#define I2C I2C1

DECLARE_I2C(SCL, B, 6, 4)
DECLARE_I2C(SDA, B, 7, 4)
DECLARE_GPIO_MOUT(RST, B, 5)

// static void write(uint8_t reg, uint8_t value) {
// 	I2C_START(I2C);

// 	I2C_ADDRESS_W(I2C1, I2C_DEVICE_ADDRESS);

// 	I2C_SEND(I2C, reg);
// 	I2C_SEND(I2C, value);

// 	I2C_STOP(I2C1);
// }

static void read_bytes(uint8_t reg, uint8_t* dst, uint16_t count) {
	I2C_START(I2C);
	I2C_ADDRESS_W(I2C, I2C_DEVICE_ADDRESS);

	I2C_SEND(I2C, reg);

	I2C_START(I2C);
	I2C_ADDRESS_R(I2C, I2C_DEVICE_ADDRESS);

	for (int i = 0; i < count; ++i) {
		if (i == count - 1) {
			I2C_ACK_OUT(I2C);
		} else {
			I2C_ACK_IN(I2C);
		}

		I2C_READ(I2C, dst[i]);
	}
	I2C_STOP(I2C);
}

void touch_init() {
	RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

	SCL_INIT();
	SDA_INIT();
	RST_INIT();

	I2C->CR1 = I2C_CR1_SWRST;
	I2C->CR1 = 0;

	I2C->CR2 |= 0x10;
	I2C->CCR |= 0x50;
	I2C->TRISE |= (11 << 0);
	I2C->CR1 |= I2C_CR1_PE;

	RST_LOW();
	delay_ms(100);
	RST_HIGH();
	delay_ms(100);
}

static void read_point(uint16_t* dst, uint8_t reg) {
	uint8_t buf[4];
	read_bytes(reg, buf, 4);
	dst[0] = (buf[0] & 0x0F) << 8 | buf[1];
	dst[1] = (buf[2] & 0x0F) << 8 | buf[3];
}

void touch_poll(uint8_t* touchCount, uint16_t* coordinates) {
	read_bytes(I2C_REG_TPC, touchCount, 1);
	if (*touchCount >= 1) read_point(coordinates, I2C_REG_T0_START);
	if (*touchCount >= 2) read_point(coordinates + 2, I2C_REG_T1_START);
}
