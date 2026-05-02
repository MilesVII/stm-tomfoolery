#pragma once

#include <stdint.h>
// #include "hal_at_home.h"

#define DIGIT_W 8
#define DIGIT_H 10

void display1_init(uint8_t isV);
uint32_t display1_setWindow(
	uint16_t x0, uint16_t y0,
	uint16_t w, uint16_t h
);
void display1_sendBytes(uint16_t* pixels, uint32_t pixelCount);
void display1_clear(uint8_t halfColor, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void display1_digit(uint16_t* gfx, char v, uint16_t atX, uint16_t atY, uint16_t backColor, uint16_t foreColor);
void display1_number(uint16_t* gfx, uint16_t v, uint16_t atX, uint16_t atY);
void display1_string(uint16_t* gfx, char* v, uint16_t atX, uint16_t atY);
