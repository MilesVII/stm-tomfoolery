#pragma once

#include <stdint.h>
// #include "hal_at_home.h"

void display_initSPI(void);
void display_init(void);
uint32_t display_setWindow(
	uint16_t x0, uint16_t y0,
	uint16_t w, uint16_t h
);
void display_sendBytes(uint16_t* pixels, uint32_t pixelCount);
void display_clear(uint8_t halfColor);
