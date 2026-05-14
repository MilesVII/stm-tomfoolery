#pragma once

#include <stdint.h>

#define TETRIS_IO_L 0b00001
#define TETRIS_IO_R 0b00010
#define TETRIS_IO_S 0b00100
#define TETRIS_IO_D 0b01000
#define TETRIS_IO_P 0b10000
#define TETRIS_SCORE_DIRTY (1 << 15)
void tetris_init(uint8_t* gfx);
void tetris_update(uint8_t* gfx, uint16_t io, float pdt, uint16_t* score);