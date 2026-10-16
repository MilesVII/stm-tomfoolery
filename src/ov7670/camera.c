#include <string.h>
#include "stm32f411xe.h"
#include "../hal_at_home.h"
#include "registers.h"
#include "camera.h"

// Wiring (see readme):
//   SCCB    PB6 SCL, PB7 SDA (I2C1, AF4)      XCLK  PB8 (TIM4 CH3)
//   RST     PB2 (active low)                  PWDN  PB1 (active high)
//   D0..D3  PA9..PA12   D4 PA15   D5..D7 PB3..PB5
//   PCLK    PA2         HREF PA3  VSYNC PA1

#define I2C          I2C1
#define SCCB_ADDR_W  0x42
#define SCCB_ADDR_R  0x43
#define I2C_TMO_MS   5

#define XCLK_HZ      12000000u

// Internal clock = XCLK / (CLKRC + 1) with the PLL bypassed; PCLK runs at that
// rate (VGA) or lower (QVGA). 0x07 -> 1.5 MHz, i.e. 56 CPU cycles per byte at
// 84 MHz, comfortable for the polling loop below when built with -O2.
// 0x05 (2 MHz) should still work; 0x0F (750 kHz) is the safe fallback.
#ifndef CAMERA_CLKRC
#define CAMERA_CLKRC 0x07
#endif

// port A bits
#define PA_VSYNC  (1u << 1)
#define PA_PCLK   (1u << 2)
#define PA_HREF   (1u << 3)

DECLARE_GPIO_MOUT(RST, B, 2)
DECLARE_GPIO_MOUT(PWDN, B, 1)
DECLARE_CLOCK(XCLK, B, 8)

static uint16_t id;

static void spin(uint32_t n) {
	while (n--) __NOP();
}

//--------------------------------------------------------------------+
// SCCB over I2C1, every wait bounded
//--------------------------------------------------------------------+

static bool i2c_wait_sr1(uint32_t mask) {
	uint32_t start = ticks;
	while (!(I2C->SR1 & mask)) {
		if (I2C->SR1 & I2C_SR1_AF) return false;       // no ACK
		if (ticks - start > I2C_TMO_MS) return false;
	}
	return true;
}

static bool i2c_wait_idle(void) {
	uint32_t start = ticks;
	while (I2C->SR2 & I2C_SR2_BUSY) {
		if (ticks - start > I2C_TMO_MS) return false;
	}
	return true;
}

static void i2c_abort(void) {
	I2C->CR1 |= I2C_CR1_STOP;
	I2C->SR1 &= ~I2C_SR1_AF;
	(void) i2c_wait_idle();
}

// START + address; on success ADDR is still pending (not cleared).
static bool i2c_start(uint8_t addr) {
	I2C->CR1 |= I2C_CR1_START;
	if (!i2c_wait_sr1(I2C_SR1_SB)) return false;
	I2C->DR = addr;
	return i2c_wait_sr1(I2C_SR1_ADDR);
}

static inline void i2c_clear_addr(void) {
	(void) I2C->SR1;
	(void) I2C->SR2;
}

// The sensor wants a little idle time between transactions.
static void sccb_gap(void) {
	spin(300);
}

static void sccb_init(void) {
	RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

	// PB6 SCL, PB7 SDA: AF4, open drain, internal pull-ups in case the module has none
	for (uint32_t pin = 6; pin <= 7; pin++) {
		MODER(GPIOB, pin, 2);
		AFR(GPIOB, pin, 4);
		OTYPER(GPIOB, pin, 1);
		OSPEEDR(GPIOB, pin, 2);
		PUPDR(GPIOB, pin, 1);
	}

	// APB1 clock from the prescaler field: 0xx = /1, 100 = /2, 101 = /4, ...
	uint32_t ppre1 = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
	uint32_t pclk1 = SystemCoreClock >> ((ppre1 & 4) ? (ppre1 & 3) + 1 : 0);
	uint32_t mhz = pclk1 / 1000000u;

	I2C->CR1 = I2C_CR1_SWRST;
	I2C->CR1 = 0;
	I2C->CR2 = mhz;                      // peripheral clock in MHz
	I2C->CCR = pclk1 / (2u * 100000u);   // standard mode, 100 kHz
	I2C->TRISE = mhz + 1;
	I2C->CR1 = I2C_CR1_PE;
}

bool camera_write(uint8_t reg, uint8_t val) {
	bool ok = false;
	if (i2c_start(SCCB_ADDR_W)) {
		i2c_clear_addr();
		I2C->DR = reg;
		if (i2c_wait_sr1(I2C_SR1_TXE)) {
			I2C->DR = val;
			ok = i2c_wait_sr1(I2C_SR1_BTF);
		}
	}
	if (ok) {
		I2C->CR1 |= I2C_CR1_STOP;
		ok = i2c_wait_idle();
	} else {
		i2c_abort();
	}
	sccb_gap();
	return ok;
}

bool camera_read(uint8_t reg, uint8_t* val) {
	// SCCB read: a write phase carrying the register index, a STOP, then a
	// separate read phase. Repeated START is not allowed.
	bool ok = false;
	if (i2c_start(SCCB_ADDR_W)) {
		i2c_clear_addr();
		I2C->DR = reg;
		ok = i2c_wait_sr1(I2C_SR1_BTF);
	}
	if (!ok) {
		i2c_abort();
		return false;
	}
	I2C->CR1 |= I2C_CR1_STOP;
	if (!i2c_wait_idle()) return false;
	sccb_gap();

	if (!i2c_start(SCCB_ADDR_R)) {
		i2c_abort();
		return false;
	}
	// Single byte: NACK it and queue the STOP right after ADDR is cleared.
	I2C->CR1 &= ~I2C_CR1_ACK;
	i2c_clear_addr();
	I2C->CR1 |= I2C_CR1_STOP;
	if (!i2c_wait_sr1(I2C_SR1_RXNE)) {
		i2c_abort();
		return false;
	}
	*val = (uint8_t) I2C->DR;
	ok = i2c_wait_idle();
	sccb_gap();
	return ok;
}

//--------------------------------------------------------------------+
// Register setup, straight from the Linux ov7670 driver
//--------------------------------------------------------------------+

typedef struct {
	uint8_t reg, val;
} regval_t;

// ov7670_default_regs: reset, VGA YUV baseline and a pile of magic numbers
// from OmniVision without which the picture is noisy and badly exposed.
static const regval_t default_regs[] = {
	{ REG_COM7, COM7_RESET },
	{ REG_CLKRC, 0x01 },
	{ REG_TSLB, 0x04 },
	{ REG_COM7, 0 },
	{ REG_HSTART, 0x13 }, { REG_HSTOP, 0x01 },
	{ REG_HREF, 0xb6 },   { REG_VSTART, 0x02 },
	{ REG_VSTOP, 0x7a },  { REG_VREF, 0x0a },
	{ REG_COM3, 0 },      { REG_COM14, 0 },
	// mystery scaling numbers
	{ REG_SCALING_XSC, 0x3a }, { REG_SCALING_YSC, 0x35 },
	{ 0x72, 0x11 }, { 0x73, 0xf0 },
	{ 0xa2, 0x02 }, { REG_COM10, 0x0 },
	// gamma curve
	{ 0x7a, 0x20 }, { 0x7b, 0x10 }, { 0x7c, 0x1e }, { 0x7d, 0x35 },
	{ 0x7e, 0x5a }, { 0x7f, 0x69 }, { 0x80, 0x76 }, { 0x81, 0x80 },
	{ 0x82, 0x88 }, { 0x83, 0x8f }, { 0x84, 0x96 }, { 0x85, 0xa3 },
	{ 0x86, 0xaf }, { 0x87, 0xc4 }, { 0x88, 0xd7 }, { 0x89, 0xe8 },
	// AGC / AEC: configure with them off, then turn them on
	{ REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT },
	{ REG_GAIN, 0 }, { REG_AECH, 0 },
	{ REG_COM4, 0x40 }, { REG_COM9, 0x18 },
	{ REG_BD50MAX, 0x05 }, { REG_BD60MAX, 0x07 },
	{ REG_AEW, 0x95 }, { REG_AEB, 0x33 },
	{ REG_VPT, 0xe3 }, { REG_HAECC1, 0x78 },
	{ REG_HAECC2, 0x68 }, { 0xa1, 0x03 },
	{ REG_HAECC3, 0xd8 }, { REG_HAECC4, 0xd8 },
	{ REG_HAECC5, 0xf0 }, { REG_HAECC6, 0x90 },
	{ REG_HAECC7, 0x94 },
	{ REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT | COM8_AGC | COM8_AEC },
	// reserved magic
	{ REG_COM5, 0x61 }, { REG_COM6, 0x4b },
	{ 0x16, 0x02 }, { REG_MVFP, 0x07 },
	{ 0x21, 0x02 }, { 0x22, 0x91 },
	{ 0x29, 0x07 }, { 0x33, 0x0b },
	{ 0x35, 0x0b }, { 0x37, 0x1d },
	{ 0x38, 0x71 }, { 0x39, 0x2a },
	{ REG_COM12, 0x78 }, { 0x4d, 0x40 },
	{ 0x4e, 0x20 }, { REG_GFIX, 0 },
	{ REG_DBLV, DBLV_X4 }, { 0x74, 0x10 },
	{ 0x8d, 0x4f }, { 0x8e, 0 },
	{ 0x8f, 0 }, { 0x90, 0 },
	{ 0x91, 0 }, { 0x96, 0 },
	{ 0x9a, 0 }, { 0xb0, 0x84 },
	{ 0xb1, 0x0c }, { 0xb2, 0x0e },
	{ 0xb3, 0x82 }, { 0xb8, 0x0a },
	// more reserved magic, some of it white balance
	{ 0x43, 0x0a }, { 0x44, 0xf0 },
	{ 0x45, 0x34 }, { 0x46, 0x58 },
	{ 0x47, 0x28 }, { 0x48, 0x3a },
	{ 0x59, 0x88 }, { 0x5a, 0x88 },
	{ 0x5b, 0x44 }, { 0x5c, 0x67 },
	{ 0x5d, 0x49 }, { 0x5e, 0x0e },
	{ 0x6c, 0x0a }, { 0x6d, 0x55 },
	{ 0x6e, 0x11 }, { 0x6f, 0x9f },
	{ 0x6a, 0x40 }, { REG_BLUE, 0x40 },
	{ REG_RED, 0x60 },
	{ REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT | COM8_AGC | COM8_AEC | COM8_AWB },
	// matrix coefficients
	{ 0x4f, 0x80 }, { 0x50, 0x80 },
	{ 0x51, 0 },    { 0x52, 0x22 },
	{ 0x53, 0x5e }, { 0x54, 0x80 },
	{ 0x58, 0x9e },
	{ REG_COM16, COM16_AWBGAIN }, { REG_EDGE, 0 },
	{ 0x75, 0x05 }, { 0x76, 0xe1 },
	{ 0x4c, 0 },    { 0x77, 0x01 },
	{ REG_COM13, 0xc3 }, { 0x4b, 0x09 },
	{ 0xc9, 0x60 }, { REG_COM16, 0x38 },
	{ 0x56, 0x40 },
	{ 0x34, 0x11 }, { REG_COM11, COM11_EXP | COM11_HZAUTO },
	{ 0xa4, 0x88 }, { 0x96, 0 },
	{ 0x97, 0x30 }, { 0x98, 0x20 },
	{ 0x99, 0x30 }, { 0x9a, 0x84 },
	{ 0x9b, 0x29 }, { 0x9c, 0x03 },
	{ 0x9d, 0x4c }, { 0x9e, 0x3f },
	{ 0x78, 0x04 },
	// some sort of multiplexor register
	{ 0x79, 0x01 }, { 0xc8, 0xf0 },
	{ 0x79, 0x0f }, { 0xc8, 0x00 },
	{ 0x79, 0x10 }, { 0xc8, 0x7e },
	{ 0x79, 0x0a }, { 0xc8, 0x80 },
	{ 0x79, 0x0b }, { 0xc8, 0x01 },
	{ 0x79, 0x0c }, { 0xc8, 0x0f },
	{ 0x79, 0x0d }, { 0xc8, 0x20 },
	{ 0x79, 0x09 }, { 0xc8, 0x80 },
	{ 0x79, 0x02 }, { 0xc8, 0xc0 },
	{ 0x79, 0x03 }, { 0xc8, 0x40 },
	{ 0x79, 0x05 }, { 0xc8, 0x30 },
	{ 0x79, 0x26 },
};

// ov7670_fmt_rgb565 with the QVGA bit folded into COM7 (COM7 does not like
// to be read back and rewritten, so it is set in one go).
static const regval_t rgb565_qvga_regs[] = {
	{ REG_COM7, COM7_RGB | COM7_FMT_QVGA },
	{ REG_COM10, 0 },              // VSYNC and HREF active high, PCLK free running
	{ REG_RGB444, 0 },
	{ REG_COM1, 0 },
	{ REG_COM15, COM15_RGB565 },
	{ REG_COM9, 0x38 },            // 16x gain ceiling
	{ 0x4f, 0xb3 }, { 0x50, 0xb3 },
	{ 0x51, 0 },    { 0x52, 0x3d },
	{ 0x53, 0xa7 }, { 0x54, 0xe4 },
	{ REG_COM13, COM13_GAMMA | COM13_UVSAT },
};

static bool write_list(const regval_t* list, uint32_t n) {
	for (uint32_t i = 0; i < n; i++) {
		if (!camera_write(list[i].reg, list[i].val)) return false;
		if (list[i].reg == REG_COM7 && (list[i].val & COM7_RESET)) delay_ms(10);
	}
	return true;
}

// Sensor window; QVGA values from the Linux driver ("empirically determined").
static bool set_window(uint32_t hstart, uint32_t hstop, uint32_t vstart, uint32_t vstop) {
	uint8_t v;
	if (!camera_write(REG_HSTART, (hstart >> 3) & 0xff)) return false;
	if (!camera_write(REG_HSTOP, (hstop >> 3) & 0xff)) return false;
	if (!camera_read(REG_HREF, &v)) return false;
	v = (v & 0xc0) | ((hstop & 0x7) << 3) | (hstart & 0x7);
	delay_ms(10);
	if (!camera_write(REG_HREF, v)) return false;

	if (!camera_write(REG_VSTART, (vstart >> 2) & 0xff)) return false;
	if (!camera_write(REG_VSTOP, (vstop >> 2) & 0xff)) return false;
	if (!camera_read(REG_VREF, &v)) return false;
	v = (v & 0xf0) | ((vstop & 0x3) << 2) | (vstart & 0x3);
	delay_ms(10);
	return camera_write(REG_VREF, v);
}

static void bus_init(void) {
	// Everything the sensor drives: plain inputs, no pulls. This also takes
	// PA15, PB3 and PB4 away from JTAG (SWD on PA13/PA14 keeps working).
	static const struct { GPIO_TypeDef* port; uint8_t pin; } pins[] = {
		{ GPIOA, 1 }, { GPIOA, 2 }, { GPIOA, 3 },
		{ GPIOA, 9 }, { GPIOA, 10 }, { GPIOA, 11 }, { GPIOA, 12 }, { GPIOA, 15 },
		{ GPIOB, 3 }, { GPIOB, 4 }, { GPIOB, 5 },
	};
	for (uint32_t i = 0; i < sizeof pins / sizeof pins[0]; i++) {
		GPIO_TypeDef* port = pins[i].port;
		uint32_t pin = pins[i].pin;
		MODER(port, pin, 0);
		PUPDR(port, pin, 0);
	}
}

uint16_t camera_id(void) {
	return id;
}

camera_status_t camera_init(void) {
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;

	RST_INIT();
	PWDN_INIT();
	PWDN_LOW();
	bus_init();
	sccb_init();
	XCLK_INIT(XCLK_HZ);

	// Hardware reset with XCLK already running; the sensor needs a clock to reset.
	RST_LOW();
	delay_ms(10);
	RST_HIGH();
	delay_ms(50);

	uint8_t pid = 0, ver = 0;
	if (!camera_read(REG_PID, &pid)) return CAMERA_ERR_SCCB;
	(void) camera_read(REG_VER, &ver);
	id = (uint16_t) (pid << 8 | ver);
	if (pid != OV7670_PID) return CAMERA_ERR_ID;

	if (!write_list(default_regs, sizeof default_regs / sizeof default_regs[0])) return CAMERA_ERR_REG;
	if (!write_list(rgb565_qvga_regs, sizeof rgb565_qvga_regs / sizeof rgb565_qvga_regs[0])) return CAMERA_ERR_REG;
	if (!set_window(168, 24, 12, 492)) return CAMERA_ERR_REG;

	// Clocking last (the Linux driver notes RGB565 looks poor otherwise):
	// PLL off so PCLK is simply XCLK / (CLKRC + 1).
	if (!camera_write(REG_DBLV, DBLV_BYPASS)) return CAMERA_ERR_REG;
	if (!camera_write(REG_CLKRC, CAMERA_CLKRC)) return CAMERA_ERR_REG;

	delay_ms(300); // let AEC/AWB settle before the first frame
	return CAMERA_OK;
}

bool camera_test_pattern(bool on) {
	// {YSC[7], XSC[7]} = 10: 8-bar color bar
	return camera_write(REG_SCALING_XSC, 0x3a) &&
	       camera_write(REG_SCALING_YSC, on ? (0x35 | TEST_PATTERN_1) : 0x35);
}

//--------------------------------------------------------------------+
// Capture
//--------------------------------------------------------------------+

// Assemble a bus byte from the two port snapshots:
// D0..D3 = PA9..PA12, D4 = PA15, D5..D7 = PB3..PB5.
static inline uint8_t bus_byte(uint32_t a, uint32_t b) {
	return (uint8_t) (((a >> 9) & 0x0F) | ((a >> 11) & 0x10) | ((b << 2) & 0xE0));
}

// RGB565 arrives high byte first: RRRRRGGG GGGBBBBB -> RRRGGGBB
static inline uint8_t rgb332(uint32_t hi, uint32_t lo) {
	return (uint8_t) ((hi & 0xE0) | ((hi & 0x07) << 2) | ((lo >> 3) & 0x03));
}

static bool wait_vsync(bool high, uint32_t timeout_ms) {
	uint32_t start = ticks;
	while (((GPIOA->IDR & PA_VSYNC) != 0) != high) {
		if (ticks - start > timeout_ms) return false;
	}
	return true;
}

// Bounded busy-wait for a PCLK level; roughly 10 ms worth of polling.
#define PCLK_SPIN 150000u

uint32_t camera_capture(uint8_t* fb) {
	// A frame starts with a VSYNC pulse. Make sure we see a whole one so we
	// never start in the middle of a frame.
	if (!wait_vsync(false, 1000)) return 0;
	if (!wait_vsync(true, 1000)) return 0;
	if (!wait_vsync(false, 100)) return 0;

	uint32_t row = 0;
	for (; row < CAMERA_H; row++) {
		uint32_t a, b, hi, lo, n;

		// Wait for the line. A new VSYNC means the frame had fewer lines than
		// expected: hand back what we have, it is still worth looking at.
		uint32_t start = ticks;
		for (;;) {
			a = GPIOA->IDR;
			if (a & PA_HREF) break;
			if (a & PA_VSYNC) return row;
			if (ticks - start > 100) return 0;
		}

		uint8_t* line = fb + row * CAMERA_W;
		uint32_t col = 0;
		for (;;) {
			// first byte: sample on the PCLK rising edge; HREF low means the line is over
			n = PCLK_SPIN;
			do { a = GPIOA->IDR; } while (!(a & PA_PCLK) && --n);
			if (!n) return 0;
			if (!(a & PA_HREF)) break;
			b = GPIOB->IDR;
			hi = bus_byte(a, b);
			n = PCLK_SPIN;
			while ((GPIOA->IDR & PA_PCLK) && --n) {}
			if (!n) return 0;

			// second byte
			n = PCLK_SPIN;
			do { a = GPIOA->IDR; } while (!(a & PA_PCLK) && --n);
			if (!n) return 0;
			b = GPIOB->IDR;
			lo = bus_byte(a, b);
			n = PCLK_SPIN;
			while ((GPIOA->IDR & PA_PCLK) && --n) {}
			if (!n) return 0;

			// Lines longer than 320 pixels (VGA, if the scaler is off) are cropped.
			if (col < CAMERA_W) line[col] = rgb332(hi, lo);
			col++;
		}
	}
	return row;
}
