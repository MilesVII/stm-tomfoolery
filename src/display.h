#pragma once

#include <stdint.h>
#include "hal_at_home.h"

void display_initSPI(void);
void display_init(void);
void display_clear(void);
void display_update(uint8_t page);
void display_update_48_32(uint8_t* frame, uint32_t status);
