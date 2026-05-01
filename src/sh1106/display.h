#pragma once

#include <stdint.h>

void display0_init(void);
void display0_clear(void);
void display0_update_48_32(uint8_t* frame, uint32_t status0, uint32_t status1);
void display0_updateTranslated(uint8_t* src);