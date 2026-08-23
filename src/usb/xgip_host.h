#ifndef XGIP_HOST_H_
#define XGIP_HOST_H_

#include "tusb.h"

#ifdef __cplusplus
 extern "C" {
#endif

// GIP button bitmask (from input report bytes 4-5, big endian)
#define XGIP_BTN_SYNC     (1U << 0)
#define XGIP_BTN_GUIDE    (1U << 1)   // Xbox logo
#define XGIP_BTN_BACK     (1U << 2)   // view
#define XGIP_BTN_START    (1U << 3)   // menu
#define XGIP_BTN_A        (1U << 4)
#define XGIP_BTN_B        (1U << 5)
#define XGIP_BTN_X        (1U << 6)
#define XGIP_BTN_Y        (1U << 7)
#define XGIP_BTN_DPAD_U   (1U << 8)
#define XGIP_BTN_DPAD_D   (1U << 9)
#define XGIP_BTN_DPAD_L   (1U << 10)
#define XGIP_BTN_DPAD_R   (1U << 11)
#define XGIP_BTN_LB       (1U << 12)
#define XGIP_BTN_RB       (1U << 13)
#define XGIP_BTN_LS       (1U << 14)
#define XGIP_BTN_RS       (1U << 15)

// True when a GIP gamepad is mounted and powered on
bool xgip_mounted(void);

// Latest button bitmask (0 if no pad / no buttons)
uint16_t xgip_buttons(void);

// True if any button is currently pressed
bool xgip_any_button_pressed(void);

// Call periodically from main loop: sends power-on / keep-alive packets
void xgip_task(void);

#ifdef __cplusplus
 }
#endif

#endif /* XGIP_HOST_H_ */
