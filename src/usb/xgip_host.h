#ifndef XGIP_HOST_H_
#define XGIP_HOST_H_

#include "tusb.h"

#ifdef __cplusplus
 extern "C" {
#endif

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
