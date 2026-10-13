#pragma once

//--------------------------------------------------------------------+
// Common
//--------------------------------------------------------------------+

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU            OPT_MCU_STM32F4
#endif

#define CFG_TUSB_OS             OPT_OS_NONE
#define CFG_TUSB_DEBUG          0

#define BOARD_TUD_RHPORT        0   // OTG_FS
#define CFG_TUD_ENABLED         1
#define CFG_TUH_ENABLED         0
#define CFG_TUD_MAX_SPEED       OPT_MODE_FULL_SPEED

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN      __attribute__((aligned(4)))

//--------------------------------------------------------------------+
// Device
//--------------------------------------------------------------------+

#define CFG_TUD_ENDPOINT0_SIZE  64

#define CFG_TUD_MSC             1
#define CFG_TUD_CDC             0
#define CFG_TUD_HID             0
#define CFG_TUD_MIDI            0
#define CFG_TUD_VENDOR          0

// One flash sector per READ10/WRITE10 callback.
#define CFG_TUD_MSC_EP_BUFSIZE  4096
