#pragma once

#include <stdint.h>

void touch_initI2C();
void touch_poll(uint8_t* touchCount, uint16_t* coordinates);
uint8_t touch_up();