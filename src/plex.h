#pragma once

#include <stdint.h>
#include <stdbool.h>

// Charlieplexed 5-pin "188" display on PA1..PA5 (L0..L4).
//
// Scanning is done one anode at a time: each plex_tick() drives one pin high,
// pulls low every pin whose LED (anode = that pin) should be lit, and floats the rest.
// 5 phases, so with a 1 ms tick the full frame refreshes at 200 Hz, 1/5 duty.

void plex_init(void);

// n: 0..199 (larger values are clamped to 199), leading zeros are blanked
void plex_show(uint8_t n, bool green, bool percent);

// call every 1 ms (from SysTick)
void plex_tick(void);

/* MAP
S_n -> pin pair (anode+ / cathode-):
S0:  L1+ L0-    S10: L0+ L1-
S1:  L2+ L1-    S11: L1+ L2-
S2:  L3+ L2-    S12: L2+ L3-
S3:  L4+ L3-    S13: L3+ L4-
S4:  L2+ L0-    S14: L0+ L2-
S5:  L3+ L1-    S15: L1+ L3-
S6:  L4+ L2-    S16: L2+ L4-
S7:  L3+ L0-    S17: L0+ L3-
S8:  L4+ L1-    S18: L1+ L4-
S9:  L4+ L0-    S19: L0+ L4-

DIGIT LEGEND:
 2
5 0
 3
6 1
 4

HUNDREDS ("1"):
0: S12
1: S15

TENS:
0: S1
1: S2
2: S11
3: S3
4: S5
5: S6
6: S8

ONES:
0: S0
1: S14
2: S10
3: S9
4: S4
5: S7
6: S17

GREEN: S16
PERCENT: S18
OFFLINE: S13, S19
*/
