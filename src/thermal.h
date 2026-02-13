#pragma once

#include <stdint.h>

void thermal_initI2C();
uint16_t thermal_poll();
