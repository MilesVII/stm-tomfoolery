#pragma once

#include <stdint.h>
#include <stdbool.h>

// OV7670 (no FIFO) driven by bit-banging the parallel bus. Output is QVGA
// RGB565 on the wire, stored as RGB332 (one byte per pixel) to fit in RAM.

#define CAMERA_W 320
#define CAMERA_H 240

typedef enum {
	CAMERA_OK = 0,
	CAMERA_ERR_SCCB,   // nothing answers on the SCCB (I2C) bus
	CAMERA_ERR_ID,     // something answered, but PID is not 0x76
	CAMERA_ERR_REG,    // a register write failed mid-configuration
} camera_status_t;

camera_status_t camera_init(void);
// PID << 8 | VER as last read during init (0x7673 for an OV7670).
uint16_t camera_id(void);

bool camera_write(uint8_t reg, uint8_t val);
bool camera_read(uint8_t reg, uint8_t* val);

// Replace the image with the sensor DSP color bars: proves the data bus and
// the capture loop without caring about optics or exposure.
bool camera_test_pattern(bool on);

// Capture one frame into fb[CAMERA_W * CAMERA_H] as RGB332. Returns the number
// of lines captured (CAMERA_H for a complete frame), 0 when no frame arrived
// in time or sync was lost.
uint32_t camera_capture(uint8_t* fb);
