#pragma once

#include <stdint.h>

#define TETRIS_IO_L  0b000000001
#define TETRIS_IO_R  0b000000010
#define TETRIS_IO_S  0b000000100
#define TETRIS_IO_D  0b000001000
#define TETRIS_IO_P  0b000010000
#define TETRIS_IO_FD 0b000100000
#define TETRIS_SCORE_DIRTY (1 << 15)
void tetris_init(uint8_t* gfx);
void tetris_update(uint8_t* gfx, uint16_t io, float pdt, uint16_t* score);