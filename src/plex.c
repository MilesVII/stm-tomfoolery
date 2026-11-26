#include "plex.h"
#include "hal_at_home.h"

DECLARE_GPIO_MOUT(L0, A, 1);
DECLARE_GPIO_MOUT(L1, A, 2);
DECLARE_GPIO_MOUT(L2, A, 3);
DECLARE_GPIO_MOUT(L3, A, 4);
DECLARE_GPIO_MOUT(L4, A, 5);

#define PINS 5

static void (*const pin_high [PINS])(void) = { L0_HIGH,  L1_HIGH,  L2_HIGH,  L3_HIGH,  L4_HIGH  };
static void (*const pin_low  [PINS])(void) = { L0_LOW,   L1_LOW,   L2_LOW,   L3_LOW,   L4_LOW   };
static void (*const pin_float[PINS])(void) = { L0_FLOAT, L1_FLOAT, L2_FLOAT, L3_FLOAT, L4_FLOAT };

// S_n -> { anode, cathode }, see MAP in plex.h
static const uint8_t seg_pins[20][2] = {
	{1, 0}, {2, 1}, {3, 2}, {4, 3}, {2, 0}, {3, 1}, {4, 2}, {3, 0}, {4, 1}, {4, 0},
	{0, 1}, {1, 2}, {2, 3}, {3, 4}, {0, 2}, {1, 3}, {2, 4}, {0, 3}, {1, 4}, {0, 4},
};

// legend segment index -> S_n
static const uint8_t hundreds_segs[2] = { 12, 15 };
static const uint8_t tens_segs[7]     = { 1, 2, 11, 3, 5, 6, 8 };
static const uint8_t ones_segs[7]     = { 0, 14, 10, 9, 4, 7, 17 };
#define SEG_GREEN   16
#define SEG_PERCENT 18

// digit -> legend segment bits (bit i = legend segment i)
static const uint8_t font[10] = {
	0x77, // 0: 0 1 2 4 5 6
	0x03, // 1: 0 1
	0x5D, // 2: 0 2 3 4 6
	0x1F, // 3: 0 1 2 3 4
	0x2B, // 4: 0 1 3 5
	0x3E, // 5: 1 2 3 4 5
	0x7E, // 6: 1 2 3 4 5 6
	0x07, // 7: 0 1 2
	0x7F, // 8: all
	0x3F, // 9: 0 1 2 3 4 5
};

// bit (anode * PINS + cathode) set = that LED is lit.
// 25 bits in one word, so plex_show() updates it atomically for the ISR.
static volatile uint32_t frame;
static uint8_t phase;

static uint32_t seg_bit(uint8_t s) {
	return 1u << (seg_pins[s][0] * PINS + seg_pins[s][1]);
}

static uint32_t digit_bits(const uint8_t *segs, uint8_t count, uint8_t pattern) {
	uint32_t bits = 0;
	for (uint8_t i = 0; i < count; ++i)
		if (pattern & (1u << i)) bits |= seg_bit(segs[i]);
	return bits;
}

void plex_init(void) {
	frame = 0;
	phase = 0;
	for (uint8_t i = 0; i < PINS; ++i) pin_float[i]();
}

void plex_show(uint8_t n, bool green, bool percent) {
	if (n > 199) n = 199;

	uint32_t f = 0;
	if (n >= 100) f |= digit_bits(hundreds_segs, 2, 0x03);
	if (n >= 10)  f |= digit_bits(tens_segs, 7, font[(n / 10) % 10]);
	f |= digit_bits(ones_segs, 7, font[n % 10]);
	if (green)   f |= seg_bit(SEG_GREEN);
	if (percent) f |= seg_bit(SEG_PERCENT);

	frame = f;
}

void plex_tick(void) {
	// float everything first so the previous phase can't ghost into this one
	for (uint8_t i = 0; i < PINS; ++i) pin_float[i]();

	uint8_t cathodes = (frame >> (phase * PINS)) & 0x1F;
	if (cathodes) {
		for (uint8_t c = 0; c < PINS; ++c)
			if (cathodes & (1u << c)) pin_low[c]();
		pin_high[phase]();
	}

	if (++phase >= PINS) phase = 0;
}
