#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// RHPort number used for host (0 = OTG_FS on STM32F4)
#ifndef BOARD_TUH_RHPORT
#define BOARD_TUH_RHPORT      0
#endif

// RHPort max operational speed
#ifndef BOARD_TUH_MAX_SPEED
#define BOARD_TUH_MAX_SPEED   OPT_MODE_FULL_SPEED
#endif

//--------------------------------------------------------------------
// Common Configuration
//--------------------------------------------------------------------

// defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

// Disable Device stack
#define CFG_TUD_ENABLED       0

// Enable Host stack
#define CFG_TUH_ENABLED       1

// Default is max speed that hardware controller could support with on-chip PHY
#define CFG_TUH_MAX_SPEED     BOARD_TUH_MAX_SPEED

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN    __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// HOST CONFIGURATION
//--------------------------------------------------------------------

// Max number of devices (directly on roothub; hub support adds more)
#define CFG_TUH_DEVICE_MAX      1

// Enable hub class driver (gamepad plugs directly, but cheap to keep off)
#define CFG_TUH_HUB             0

// Built-in HID host driver: our gamepad is NOT HID (vendor GIP protocol),
// so keep it disabled and use the custom XGIP driver instead.
#define CFG_TUH_HID             0

#define CFG_TUH_CDC             0
#define CFG_TUH_MSC             0
#define CFG_TUH_MIDI            0
#define CFG_TUH_VENDOR          0

// Custom Xbox GIP (XInput-like) host driver
#define CFG_TUH_XGIP            1

// Buffer size for GIP interrupt endpoint transfers
#define CFG_TUH_XGIP_EP_BUFSIZE 64

#ifdef __cplusplus
 }
#endif

#endif /* TUSB_CONFIG_H_ */
