#pragma once

#include <stdint.h>

// y-up
#define FONT_0 \
	0b0111, \
	0b0101, \
	0b0101, \
	0b0101, \
	0b0111, \
	0b0000, \
	0b0000
#define FONT_1 \
	0b0001, \
	0b0001, \
	0b0001, \
	0b0001, \
	0b0001, \
	0b0000, \
	0b0000
#define FONT_2 \
	0b0111, \
	0b0001, \
	0b0111, \
	0b0100, \
	0b0111, \
	0b0000, \
	0b0000
#define FONT_3 \
	0b0111, \
	0b0100, \
	0b0111, \
	0b0100, \
	0b0111, \
	0b0000, \
	0b0000
#define FONT_4 \
	0b0100, \
	0b0100, \
	0b0111, \
	0b0101, \
	0b0110, \
	0b0000, \
	0b0000
#define FONT_5 \
	0b0111, \
	0b0100, \
	0b0111, \
	0b0001, \
	0b0111, \
	0b0000, \
	0b0000
#define FONT_6 \
	0b0111, \
	0b0101, \
	0b0111, \
	0b0001, \
	0b0111, \
	0b0000, \
	0b0000
#define FONT_7 \
	0b0010, \
	0b0010, \
	0b0010, \
	0b0100, \
	0b0111, \
	0b0000, \
	0b0000
#define FONT_8 \
	0b0111, \
	0b0101, \
	0b0111, \
	0b0101, \
	0b0111, \
	0b0000, \
	0b0000
#define FONT_9 \
	0b0111, \
	0b0100, \
	0b0111, \
	0b0101, \
	0b0111, \
	0b0000, \
	0b0000

#define CHAR(n) { FONT_##n }

const uint8_t CHARACTERS[10][7] = {
	CHAR(0),
	CHAR(1),
	CHAR(2),
	CHAR(3),
	CHAR(4),
	CHAR(5),
	CHAR(6),
	CHAR(7),
	CHAR(8),
	CHAR(9)
};
