#include <stdint.h>
#include <stdbool.h>
#include <math.h>

// utils, move later
#define STRUCT(name, fields) \
	struct name { \
		fields \
	}; \
	typedef struct name name;
#define FILL(v, d) for (int i = 0; i < sizeof(v); ++i) { v[i] = d; }
#define AT(x, y, w) ((x) + (y) * (w))

static uint8_t tetris_render(uint8_t x, uint8_t y);

#define SW 64 // screen size
#define SH 128
#define GW 12 // glass size
#define GH 22
#define GHR 20 // glass height rendered
#define CELL_SIZE 4

// glass origin with grid
static const uint8_t GORGX = (SW - (CELL_SIZE + 1) * GW - 1) / 2;
static const uint8_t GORGY = (SH - (CELL_SIZE + 1) * GH - 1) / 2;

bool cells[GW * GH];
float lineClears[GH];

void tetris_init() {
	FILL(cells, 0);
	FILL(lineClears, 0.0);
}

void tetris_update(uint8_t* gfx) {
	//debug
	lineClears[19] += .04;
	if (lineClears[19] > 1) lineClears[19] = 0.0;

	for (uint16_t i = 0; i < 1024; ++i) {
		uint8_t target = 0;
		uint16_t y = i / 8;
		uint16_t xB = i - y * 8;
		for (uint8_t bit = 0; bit < 8; ++bit) {
			uint16_t x = xB * 8 + bit;
			target |= tetris_render(x, y) << bit;
		}
		gfx[i] = target;
	}
}

static uint8_t tetris_render(uint8_t x, uint8_t y) {
	if (x >= GORGX && y >= GORGY) {
		// subcoords
		uint8_t sx = (x - GORGX) % (CELL_SIZE + 1);
		uint8_t sy = (y - GORGY) % (CELL_SIZE + 1);
		// dimming exclusion
		uint8_t dxx = y > GORGY && (sx == 1 || sx == 4); // 2 || 3 for cross-shaped dimming
		uint8_t dxy = x > GORGX && (sy == 1 || sy == 4); // 2 || 3 for cross-shaped dimming
		// grid high boundary
		uint8_t hbx = GORGX + GW  * (CELL_SIZE + 1);
		uint8_t hby = GORGY + GHR * (CELL_SIZE + 1);
		if (x == hbx && y <= hby) return 1;
		if (y == hby && x <= hbx) return 1;

		uint8_t cx = (x - GORGX) / (CELL_SIZE + 1);
		uint8_t cy = (y - GORGY) / (CELL_SIZE + 1);
		if (cx < GW && cy < GH) {
			// grid
			if (cy < GHR) {
				if (sx == 0) return !dxy;
				if (sy == 0) return !dxx;
			}

			// line clear animation
			if (lineClears[cy]) {
				double eh;
				float v = modf((x + y) / (float)(CELL_SIZE + 1), &eh);
				return (v) > lineClears[cy];
			}

			if (cells[AT(cx, cy, GW)]) return 1;
		}
	}

	return 0;
}

