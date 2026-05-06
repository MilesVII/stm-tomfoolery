#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

// utils, move later
#define STRUCT(name, fields) \
	struct name { \
		fields \
	}; \
	typedef struct name name;
#define ENUM(name, ...) \
	enum name { \
		__VA_ARGS__ \
	}; \
	typedef enum name name;

#define FILL(v, d) for (int i = 0; i < (sizeof(v) / sizeof(v[0])); ++i) { v[i] = d; }
#define AT(x, y, w) ((x) + (y) * (w))

static uint8_t tetris_drawGrid(uint8_t x, uint8_t y);
static void pickTetramino();

#define SW 64 // screen size
#define SH 128
#define GW 12 // glass size
#define GH 22
#define GHR 20 // glass height rendered
#define CELL_SIZE 4

// glass origin with grid
static const uint8_t GORGX = (SW - (CELL_SIZE + 1) * GW - 1) / 2;
static const uint8_t GORGY = (SH - (CELL_SIZE + 1) * GH - 1) / 2;

STRUCT(V2, uint8_t x; uint8_t y;)
ENUM(
	Tetramino,
	TETRA_I,
	TETRA_O,
	TETRA_T,
	TETRA_S,
	TETRA_Z,
	TETRA_L,
	TETRA_J
)
uint8_t TM_I[] = {
	0, 0, 1, 0,
	0, 0, 1, 0,
	0, 0, 1, 0,
	0, 0, 1, 0
};
uint8_t TM_O[] = {
	0, 0, 0, 0,
	0, 1, 1, 0,
	0, 1, 1, 0,
	0, 0, 0, 0
};
uint8_t TM_T[] = {
	0, 1, 0,
	1, 1, 1,
	0, 0, 0
};
uint8_t TM_S[] = {
	0, 1, 0,
	0, 1, 1,
	0, 0, 1
};
uint8_t TM_Z[] = {
	0, 1, 0,
	1, 1, 0,
	1, 0, 0
};
uint8_t TM_L[] = {
	0, 1, 0,
	0, 1, 0,
	0, 1, 1
};
uint8_t TM_J[] = {
	0, 1, 0,
	0, 1, 0,
	1, 1, 0
};
uint8_t* tetraminoCells[] = {
	TM_I,
	TM_O,
	TM_T,
	TM_S,
	TM_Z,
	TM_L,
	TM_J
};
Tetramino tetraminoFeed[] = {
	TETRA_I,
	TETRA_O,
	TETRA_T,
	TETRA_S,
	TETRA_Z,
	TETRA_L,
	TETRA_J
};
STRUCT(CellRenderOffset,
	uint8_t page;
	uint8_t b0;
	uint8_t b1;
);
CellRenderOffset CELL_RENDER_OFFSETS[GW];
uint8_t tetraminoFeedPointer = 0;
const V2 TETRAMINO_SPAWN = { GW / 2 - 3, GH - 1 };
V2 tetraminoPosition;
bool cells[GW * GH];
bool tetramino[4*4];
float lineClears[GH];
bool coldStart = true;

void tetris_init(uint8_t* gfx) {
	FILL(cells, 0);
	FILL(lineClears, 0.0);

	for (uint16_t i = 0; i < 1024; ++i) {
		uint8_t target = 0;
		uint16_t y = i / 8;
		uint16_t xB = i - y * 8;
		for (uint8_t bit = 0; bit < 8; ++bit) {
			uint16_t x = xB * 8 + bit;
			target |= tetris_drawGrid(x, y) << bit;
		}
		gfx[i] = target;
	}

	for (uint8_t cx = 0; cx < GW; ++cx) {
		uint8_t x = GORGX + cx * (CELL_SIZE + 1) + 1;
		uint8_t slide = x % 8;
		CELL_RENDER_OFFSETS[cx].page = x / 8;
		CELL_RENDER_OFFSETS[cx].b0 = 0xF0 >> slide;
		CELL_RENDER_OFFSETS[cx].b1 = 0xF0 << -(slide - 8);
	}
}

void tetris_update(uint8_t* gfx, uint8_t io, float pdt) {
	lineClears[10] += .04;
	if (lineClears[10] > 1.0) lineClears[10] = 0.0;
	if (coldStart) {
		pickTetramino();
		coldStart = false;
	}

	for (uint8_t cy = 0; cy < GH; ++cy)
	for (uint8_t cx = 0; cx < GW; ++cx) {
		bool cv = cells[AT(cx, cy, GW)];
		uint8_t fromx = GORGX + cx * (CELL_SIZE + 1);
		uint8_t fromy = GORGY + cy * (CELL_SIZE + 1);

		if (lineClears[cy]) {
			for (uint8_t y = fromy; y < fromy + CELL_SIZE + 1; ++y)
			for (uint8_t x = fromx; x < fromx + CELL_SIZE + 1; ++x) {
				double eh;
				float v = modf((x + y) / (float)(CELL_SIZE + 1), &eh);
				return (v) > lineClears[cy];
			}
		} else {
			for (int y = fromy; y < (fromy + CELL_SIZE); ++y) {
				int ix = AT(CELL_RENDER_OFFSETS[cx].page, fromy, SW);
				gfx[ix]     &= ~CELL_RENDER_OFFSETS[cx].b0;
				gfx[ix]     |= CELL_RENDER_OFFSETS[cx].b0;
				gfx[ix + 1] &= ~CELL_RENDER_OFFSETS[cx].b1;
				gfx[ix + 1] |= CELL_RENDER_OFFSETS[cx].b1;
			}
		}
	}
}

static uint8_t tetris_drawGrid(uint8_t x, uint8_t y) {
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
			if (cy < GHR) {
				if (sx == 0) return !dxy;
				if (sy == 0) return !dxx;
			}
		}
	}

	return 0;
}

static void loadTetramino(Tetramino t) {
	// y-down
	uint8_t sourceSide = t == TETRA_I || t == TETRA_O ? 4 : 3;
	uint8_t* src = tetraminoCells[t];
	for (uint8_t y = 0; y < 4; ++y)
	for (uint8_t x = 0; x < 4; ++x) {
		uint8_t i = x + y * 4;
		if ((x == 3 || y == 3) && sourceSide == 4) {
			tetramino[i] = 0;
			continue;
		}
		tetramino[i] = src[x + y * sourceSide];
	}
}
static void pickTetramino() {
	uint8_t count = sizeof(tetraminoFeed) / sizeof(tetraminoFeed[0]);
	if (tetraminoFeedPointer == 0) {
		// shuffle
		for (int i = count - 1; i >= 0; --i) {
			int j = rand() % (i + 1);
			Tetramino pocket = tetraminoFeed[i];
			tetraminoFeed[i] = tetraminoFeed[j];
			tetraminoFeed[j] = pocket;
		}
	}

	Tetramino target = tetraminoFeed[tetraminoFeedPointer++];
	if (tetraminoFeedPointer >= count) tetraminoFeedPointer = 0;
	loadTetramino(target);
	tetraminoPosition = TETRAMINO_SPAWN;
}
