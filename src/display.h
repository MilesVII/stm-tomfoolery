#pragma once

#include <stdint.h>
#include "hal_at_home.h"

void display_initSPI(void);
void display_init(void);
void display_clear(void);
void display_update(/*const UBYTE *Image*/);
