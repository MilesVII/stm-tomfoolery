#pragma once

#include <stdint.h>

void touch_initSPI(void);
uint32_t touch_poll();
uint8_t touch_up();