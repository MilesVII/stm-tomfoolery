#pragma once

#include <stdint.h>

#define TETRIS_IO_L 0b0001
#define TETRIS_IO_R 0b0010
#define TETRIS_IO_S 0b0100
#define TETRIS_IO_D 0b1000

void tetris_init();
void tetris_update(uint8_t* gfx, uint8_t io, float pdt);