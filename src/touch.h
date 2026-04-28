#pragma once

#include <stdint.h>

void touch_init();
void touch_poll(uint8_t* touchCount, uint16_t* coordinates);