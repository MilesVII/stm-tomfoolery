#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "tetris.h"

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

static uint8_t drawGrid(uint8_t x, uint8_t y);
static void pickTetramino();
static bool isLegal(int8_t dx, int8_t dy);
static void collapse(uint8_t y);
static void bake();
static void spin();

#define SW 64 // screen size
#define SWP (64/8) // (in pages)
#define SH 128
#define GW 12 // glass size
#define GH 22
#define GHR 20 // glass height rendered
#define TCAW 4 // tetramino cell array width
#define CELL_SIZE 4
#define THB_MS 0.500 // tetramino hover base ms
#define ICR_MS 0.050 // IO clock rate ms
#define LCR_MS 0.800 // line clear rate ms

// glass origin with grid
static const uint8_t GORGX = (SW - (CELL_SIZE + 1) * GW - 1) / 2;
static const uint8_t GORGY = (SH - (CELL_SIZE + 1) * GH - 1) / 2;

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
CellRenderOffset CRO[GW];
STRUCT(V2,
	int8_t x;
	int8_t y;
);
STRUCT(TetraminoState,
	bool cells[TCAW * TCAW];
	V2 position;
	uint8_t side;
	Tetramino type;
	float hover;
);
TetraminoState tetramino;
const V2 TETRAMINO_SPAWN = { GW / 2 - 3, GH - 4 };

// tetramino feed
uint8_t tetraminoFeedPointer = 0;

// glass
bool cells[GW * GH];
bool collapsing[GH];
float lcp = 0; // line collapse progress

float ioCLK = 0;
bool gameOver = false;

void tetris_init(uint8_t* gfx) {
	FILL(cells, 0);
	FILL(collapsing, false);

	for (uint16_t i = 0; i < 1024; ++i) {
		uint8_t target = 0;
		uint16_t y = i / 8;
		uint16_t xB = i - y * 8;
		for (uint8_t bit = 0; bit < 8; ++bit) {
			uint16_t x = xB * 8 + bit;
			target |= drawGrid(x, y) << bit;
		}
		gfx[i] = target;
	}

	for (uint8_t cx = 0; cx < GW; ++cx) {
		uint8_t x = GORGX + cx * (CELL_SIZE + 1) + 1;
		uint8_t slide = x % 8;
		CRO[cx].page = x / 8;
		CRO[cx].b0 = 0x0F << slide;
		CRO[cx].b1 = 0x0F >> -(slide - 8);
	}

	pickTetramino();
}

void tetris_update(uint8_t* gfx, uint8_t io, float pdt) {
	ioCLK += pdt;
	if (ioCLK >= ICR_MS) {
		ioCLK -= ICR_MS;
	} else {
		io = 0x00;
	}

	if (!lcp) {
		if ((io & TETRIS_IO_L) && isLegal(-1, 0)) tetramino.position.x -= 1;
		if ((io & TETRIS_IO_R) && isLegal( 1, 0)) tetramino.position.x += 1;
		if ((io & TETRIS_IO_S)) {
			spin();
			tetramino.hover = THB_MS;
			// jiggle + test
		}
		tetramino.hover -= pdt;
		if (io & TETRIS_IO_D) tetramino.hover = 0;
	} else {
		lcp -= pdt;
		if (lcp <= 0) {
			lcp = 0;
			for (int8_t y = GH - 2; y >= 0; --y) {
				if (collapsing[y]) {
					collapsing[y] = false;
					collapse(y);
				}
			}
			pickTetramino();
		}
	}

	if (tetramino.hover <= 0) {
		if (isLegal(0, -1)) {
			tetramino.position.y -= 1;
			tetramino.hover = THB_MS;
		} else {
			bake();
			if (!lcp) pickTetramino();
		}
	}

	for (uint8_t cy = 0; cy < GH; ++cy)
	for (uint8_t cx = 0; cx < GW; ++cx) {
		bool cv = cells[AT(cx, cy, GW)];
		if (
			!cv &&
			cx >= tetramino.position.x &&
			cy >= tetramino.position.y &&
			cx < tetramino.position.x + TCAW &&
			cy < tetramino.position.y + TCAW
		) {
			uint8_t tcix = AT(
				cx - tetramino.position.x,
				cy - tetramino.position.y,
				TCAW
			);
			cv = tetramino.cells[tcix];
		}
		uint8_t fromy = GORGY + cy * (CELL_SIZE + 1) + 1;

		for (int y = 0; y < CELL_SIZE; ++y) {
			if (collapsing[cy]) cv = 1.0 - (float)y / CELL_SIZE > lcp;
			int ix = AT(CRO[cx].page, fromy + y, SWP);
					gfx[ix]     &= ~CRO[cx].b0;
			if (cv) gfx[ix]     |=  CRO[cx].b0;
					gfx[ix + 1] &= ~CRO[cx].b1;
			if (cv) gfx[ix + 1] |=  CRO[cx].b1;
		}
	}
}

static uint8_t drawGrid(uint8_t x, uint8_t y) {
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

	// load tetramino, y-down
	uint8_t sourceSide = target == TETRA_I || target == TETRA_O ? 4 : 3;
	uint8_t* src = tetraminoCells[target];
	for (uint8_t y = 0; y < TCAW; ++y)
	for (uint8_t x = 0; x < TCAW; ++x) {
		uint8_t ix = AT(x, y, TCAW);
		if ((x >= 3 || y >= 3) && sourceSide == 3) {
			tetramino.cells[ix] = 0;
		} else
			tetramino.cells[ix] = src[x + y * sourceSide];
	}

	tetramino.position = TETRAMINO_SPAWN;
	tetramino.side = sourceSide;
	tetramino.type = target;
	tetramino.hover = THB_MS;
}

static bool isLegal(int8_t dx, int8_t dy) {
	for (uint8_t ty = 0; ty < TCAW; ++ty)
	for (uint8_t tx = 0; tx < TCAW; ++tx) {
		bool tcv = tetramino.cells[AT(tx, ty, TCAW)];
		if (!tcv) continue;
		// hbound check
		if (tx + tetramino.position.x + dx < 0) return false;
		if (tx + tetramino.position.x + dx >= GW) return false;

		// bottom check
		if (ty + tetramino.position.y + dy < 0) return false;

		// collision check
		bool cv = cells[AT(
			tx + tetramino.position.x + dx,
			ty + tetramino.position.y + dy,
			GW
		)];
		if (cv) return false;
	}
	return true;
}

static void bake() {
	for (uint8_t ty = 0; ty < TCAW; ++ty)
	for (uint8_t tx = 0; tx < TCAW; ++tx) {
		bool tcv = tetramino.cells[AT(tx, ty, TCAW)];
		if (!tcv) continue;
		if (ty + tetramino.position.y > GH) gameOver = true;
		cells[AT(
			tx + tetramino.position.x,
			ty + tetramino.position.y,
			GW
		)] = tcv;
	}
	for (uint8_t cy = 0; cy < GH; ++cy) {
		for (uint8_t cx = 0; cx < GW; ++cx) {
			if (!cells[AT(cx, cy, GW)]) goto ROW_INCOMPLETE;
		}
		collapsing[cy] = true;
		lcp = LCR_MS;
ROW_INCOMPLETE:
	}
}

static void collapse(uint8_t y) {
	for (uint8_t cy = y; cy < GH - 1; ++cy)
	for (uint8_t cx = 0; cx < GW; ++cx) {
		cells[AT(cx, cy, GW)] = cells[AT(cx, cy + 1, GW)];
	}
}

bool sb[TCAW * TCAW]; // spin buffer
static void spin() {
	FILL(sb, false);
	uint8_t s = tetramino.side;
	for (uint8_t y = 0; y < TCAW; ++y)
	for (uint8_t x = 0; x < TCAW; ++x) {
		// transpose
		sb[AT(x, y, TCAW)] = tetramino.cells[AT(y, x, TCAW)];
	}
	for (uint8_t y = 0; y < TCAW; ++y)
	for (uint8_t x = 0; x < TCAW; ++x) {
		// flip
		tetramino.cells[AT(x, y, TCAW)] = sb[AT(TCAW - x - 1, y, TCAW)];
	}
}
