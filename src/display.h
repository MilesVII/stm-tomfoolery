#pragma once

#include <stdint.h>

#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

#define OLED_DC_Pin            GPIO_PIN_8
#define OLED_DC_GPIO_Port      GPIOA
#define OLED_RST_Pin           GPIO_PIN_1 // changed from A9 to A1 according to blackpill docs
#define OLED_RST_GPIO_Port     GPIOA
#define OLED_CS_Pin            GPIO_PIN_6
#define OLED_CS_GPIO_Port      GPIOB

#define OLED_CS_0 PIN_CLR(OLED_CS_GPIO_Port, OLED_CS_Pin)
#define OLED_CS_1 PIN_SET(OLED_CS_GPIO_Port, OLED_CS_Pin)

#define OLED_DC_0 PIN_CLR(OLED_DC_GPIO_Port, OLED_DC_Pin)
#define OLED_DC_1 PIN_SET(OLED_DC_GPIO_Port, OLED_DC_Pin)

#define OLED_RST_0 PIN_CLR(OLED_RST_GPIO_Port, OLED_RST_Pin)
#define OLED_RST_1 PIN_SET(OLED_RST_GPIO_Port, OLED_RST_Pin)

void display_initSPI(void);
void display_init(void);
void display_clear(void);
void display_update(/*const UBYTE *Image*/);
