#include "stm32f411xe.h"
#include "../hal_at_home.h"
#include "display.h"
#include "font.h"
#include <math.h>
#include <stdarg.h>
#include <string.h>

// 240x320
#define SPI SPI2
#define DMA_SPI_TX DMA1_Stream4
#define DMA_SPI_TX_IRQn DMA1_Stream4_IRQn
#define DMA_SPI_TX_IRQHandler DMA1_Stream4_IRQHandler

DECLARE_SPI(SCK , B, 13, 5);
DECLARE_SPI(MOSI, B, 15, 5);
DECLARE_GPIO_MOUT(NSS, A, 8);
DECLARE_GPIO_MOUT(RST, B, 12);
DECLARE_GPIO_MOUT(DC , B, 14);

static void initSPI() {
	RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

	SCK_INIT();
	MOSI_INIT();
	NSS_INIT();
	DC_INIT();
	RST_INIT();

	// clear
	SPI->CR1 = 0;
	SPI->CR2 = 0;

	SPI->CR1 =
		SPI_CR1_MSTR |
		SPI_CR1_SSI  |
		SPI_CR1_SSM  |
		0; //BR /2
	SPI->CR1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA);
	
	SPI->CR1 |= SPI_CR1_SPE;
}

static void initDMA(void);

static void SPI_Transfer(uint8_t data) {
	while (!SPI_TXE_READY(SPI));
	SPI->DR = data;
	while (!SPI_RXNE_READY(SPI));
	(void)SPI->DR;
	(void)SPI->SR;
}
static void SPI_Write(uint8_t *data, uint32_t len) {
	NSS_LOW();

	for (uint32_t i = 0; i < len; i++) {
		SPI_Transfer(data[i]);
	}
	while (SPI_BSY(SPI));

	NSS_HIGH();
}
static void SPI_WriteFill(uint8_t data, uint32_t len) {
	NSS_LOW();

	for (uint32_t i = 0; i < len; i++) {
		SPI_Transfer(data);
	}
	while (SPI_BSY(SPI));

	NSS_HIGH();
}

static void reg(uint8_t command) {
	DC_LOW();
	SPI_Write(&command, 1);
}
static void stream(uint8_t* byte, uint32_t count) {
	DC_HIGH();
	SPI_Write(byte, count);
}
static void fill(uint8_t byte, uint32_t count) {
	DC_HIGH();
	SPI_WriteFill(byte, count);
}
static void data(uint8_t byte) {
	DC_HIGH();
	SPI_Write(&byte, 1);
}
static void command(int count, ...) {
	va_list args;
	va_start(args, count);

	for (int i = 0; i < count; i++) {
		uint8_t byte = (uint8_t)va_arg(args, int);
		if (i == 0) reg(byte);
		else data(byte);
	}

	va_end(args);
}

void display1_init(uint8_t isV) {
	initSPI();
	initDMA();
	RST_HIGH();
	delay_ms(100);
	RST_LOW();
	delay_ms(100);
	RST_HIGH();
	delay_ms(100);

	reg(0x01); // SWRESET;
	delay_ms(150);

	reg(0x28); // off

	if (isV) reg(0x21); // color inversion
	// command(2, 0xC0, 0x23); // power control A
	// command(2, 0xC1, ____); // power control B
	// command(3, 0xC5, 0x3E, 0x28); // vcom control 1
	// command(2, 0xC7, 0x86); // vcom control 2

	// command(4, 0xE8, 0x85, 0x00, 0x78); // driver timing control A
	// command(5, 0xED, 0x64, 0x03, 0x12, 0x81); // power on sequence control
	// command(16, 0xE0, 0x0F, 0x29, 0x24, 0x0C, 0x0E, 0x09, 0x4E, 0x78, 0x3C, 0x09, 0x13, 0x05, 0x17, 0x11, 0x00);
	// command(16, 0xE1, 0x00, 0x16, 0x1B, 0x04, 0x11, 0x07, 0x31, 0x33, 0x42, 0x05, 0x0C, 0x0A, 0x28, 0x2F, 0x0F);

	command(2, 0x3A, 0x55);       // RGB565
	command(2, 0x36, 0b00001000); // memory access control

	reg(0x11); // sleep out
	delay_ms(120);
	reg(0x29); // on
}

#define HI(v) ((v) >> 8)
#define LO(v) ((v) & 0xFF)
#define SEND16(v) \
	data(HI(v)); \
	data(LO(v));
uint32_t display1_setWindow(
	uint16_t x0, uint16_t y0,
	uint16_t w, uint16_t h
) {
	reg(0x2A); // column
	SEND16(x0);
	SEND16(x0 + w - 1);
	reg(0x2B); // row
	SEND16(y0);
	SEND16(y0 + h - 1);
	return w * h;
}

void display1_sendBytes(uint16_t* pixels, uint32_t pixelCount) {
	reg(0x2C);
	stream((uint8_t*)pixels, pixelCount * 2);
}

void display1_sendBytesIndexed(const uint8_t* pixels, const uint16_t* palette, uint32_t pixelCount) {
	reg(0x2C);
	DC_HIGH();
	NSS_LOW();

	for (uint32_t i = 0; i < pixelCount; i++) {
		uint16_t v = palette[pixels[i]];
		SPI_Transfer(v >> 8);
		SPI_Transfer(v & 0xFF);
	}
	while (SPI_BSY(SPI));

	NSS_HIGH();
}

// ---------------------------------------------------------------------------
// DMA support
//
// SPI2 TX is on DMA1 Stream 4 Channel 0 (RM0090 rev19 Table 43).
// We use a small static line buffer and feed the DMA in chunks so the source
// buffer can live in flash (no need for a full 240*320*2 RAM framebuffer).
// ---------------------------------------------------------------------------

#define DMA_LINE_PIXELS 240
static uint16_t dma_line_buffer[DMA_LINE_PIXELS];
static volatile uint8_t dma_busy;

static void initDMA(void) {
	RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

	DMA_SPI_TX->CR = 0;
	while (DMA_SPI_TX->CR & DMA_SxCR_EN);

	DMA_SPI_TX->PAR = (uint32_t)&SPI->DR;
	DMA_SPI_TX->M0AR = (uint32_t)dma_line_buffer;
	DMA_SPI_TX->CR =
		(0 << DMA_SxCR_CHSEL_Pos) |   // channel 0
		DMA_SxCR_PL_1 |               // high priority
		DMA_SxCR_MINC |               // memory increment
		DMA_SxCR_DIR_1 |              // memory -> peripheral
		DMA_SxCR_TCIE;                // transfer complete interrupt

	NVIC_EnableIRQ(DMA_SPI_TX_IRQn);
}

void DMA_SPI_TX_IRQHandler(void) {
	if (DMA1->HISR & DMA_HISR_TCIF4) {
		DMA1->HIFCR = DMA_HIFCR_CTCIF4;
		SPI->CR2 &= ~SPI_CR2_TXDMAEN;
		while (SPI_BSY(SPI));
		NSS_HIGH();
		dma_busy = 0;
	}
}

void display1_waitDMA(void) {
	while (dma_busy) {
		__NOP();
	}
}

void display1_sendBytesDMA(const uint16_t* pixels, uint32_t pixelCount) {
	display1_waitDMA();

	reg(0x2C);
	DC_HIGH();

	dma_busy = 1;
	NSS_LOW();

	while (DMA_SPI_TX->CR & DMA_SxCR_EN);
	DMA_SPI_TX->M0AR = (uint32_t)pixels;
	DMA_SPI_TX->NDTR = pixelCount * 2;
	DMA_SPI_TX->CR =
		(0 << DMA_SxCR_CHSEL_Pos) |
		DMA_SxCR_PL_1 |
		DMA_SxCR_MINC |
		DMA_SxCR_DIR_1 |
		DMA_SxCR_TCIE |
		DMA_SxCR_EN;

	SPI->CR2 |= SPI_CR2_TXDMAEN;
}

void display1_sendBytesIndexedDMA(const uint8_t* pixels, const uint16_t* palette, uint32_t pixelCount) {
	reg(0x2C);
	DC_HIGH();

	dma_busy = 1;
	NSS_LOW();

	uint32_t sent = 0;
	while (sent < pixelCount) {
		uint32_t chunk = pixelCount - sent;
		if (chunk > DMA_LINE_PIXELS) chunk = DMA_LINE_PIXELS;

		for (uint32_t i = 0; i < chunk; i++) {
			dma_line_buffer[i] = palette[pixels[sent + i]];
		}

		display1_waitDMA();
		// Note: waitDMA resets dma_busy to 0 after first iteration, keep it set.
		dma_busy = 1;

		while (DMA_SPI_TX->CR & DMA_SxCR_EN);
		DMA_SPI_TX->M0AR = (uint32_t)dma_line_buffer;
		DMA_SPI_TX->NDTR = chunk * 2;
		DMA_SPI_TX->CR =
			(0 << DMA_SxCR_CHSEL_Pos) |
			DMA_SxCR_PL_1 |
			DMA_SxCR_MINC |
			DMA_SxCR_DIR_1 |
			DMA_SxCR_TCIE |
			DMA_SxCR_EN;

		SPI->CR2 |= SPI_CR2_TXDMAEN;

		sent += chunk;
	}

	display1_waitDMA();
}

void display1_clear(uint8_t halfColor, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
	uint32_t count = display1_setWindow(x, y, w, h);
	reg(0x2C);
	fill(halfColor, count * 2);
}

void display1_digit(uint16_t* gfx, char v, uint16_t atX, uint16_t atY, uint16_t backColor, uint16_t foreColor) {
	uint16_t* cursor = gfx;

	uint32_t pc = display1_setWindow(atX, atY, DIGIT_W, DIGIT_H);
	for (int y = DIGIT_H - 1; y >= 0; --y) {
		uint8_t row = GET_CHARACTER(v)[y/2];
		for (int x = 0; x < DIGIT_W; ++x) {
			uint8_t bright = row & (1 << (3 - x/2));
			*cursor = bright ? foreColor : backColor;
			++cursor;
		}
	}
	display1_sendBytes(gfx, pc);
}

void display1_number(uint16_t* gfx, uint16_t v, uint16_t atX, uint16_t atY) {
	display1_clear(0x0F, 0, atY, 240, DIGIT_H);
	while(1) {
		atX -= DIGIT_W;
		uint8_t digit = v % 10;
		display1_digit(gfx, digit, atX, atY, 0x0000, 0xFFFF);
		v /= 10;
		if (v <= 0) return;
	}
}
void display1_string(uint16_t* gfx, char* v, uint16_t atX, uint16_t atY) {
	uint16_t l = strlen(v);
	for (uint16_t i = 0; i < l; ++i) {
		display1_digit(gfx, v[i], atX + DIGIT_W * i, atY, 0x0000, 0xFFFF);
	}
}
void display1_button(uint16_t* gfx, char* caption, uint16_t atX, uint16_t atY, uint16_t w, uint16_t h) {
	for (int y = 0; y < h; ++y)
	for (int x = 0; x < w; ++x) {
		if (y <= 2 || y >= h - 3 || x <= 2 || x >= w - 3) {
			gfx[x + y * w] = 0x00FA;
		} else {
			gfx[x + y * w] = 0x0000;
		}
	}
	display1_sendBytes(gfx, display1_setWindow(atX, atY, w, h));

	uint16_t tx = atX + (w - DIGIT_W * strlen(caption)) / 2 ;
	uint16_t ty = atY + (h - DIGIT_H) / 2;
	display1_string(gfx, caption, tx, ty);
}
