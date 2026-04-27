#pragma once

#include <stdint.h>

void touch_initSPI();
void touch_poll(uint16_t* x, uint16_t* y);
uint8_t touch_up();
void touch_calibrate(uint16_t* x, uint16_t* y);
