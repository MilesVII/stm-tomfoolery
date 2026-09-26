#pragma once

// screen size
#define SW 240
#define SH 320
// button size
#define BS 64

#define RECT_M            0, (SH - BS)          , BS, BS
#define RECT_P    (SW - BS), (SH - BS)          , BS, BS
#define RECT_TARG        BS, (SH - BS)          , (SW - BS * 2), BS
// #define RECT_TRED         0, (SH - BS - DIGIT_H * 2), SW, DIGIT_H
// #define RECT_BATT         0, (SH - BS - DIGIT_H * 4), SW, DIGIT_H
#define RECT_L(line)      0, (SH - BS - DIGIT_H * ((line + 1) * 2)), SW, DIGIT_H

#define IO_OFF_M 0
#define IO_OFF_P 1

#define IO_TAP(io, K) ( io & (~io >> 4) & 1 << IO_OFF_##K)
#define IO_RSE(io, K) (~io & ( io >> 4) & 1 << IO_OFF_##K)
#define IO_HLD(io, K) ( io & ( io >> 4) & 1 << IO_OFF_##K)

#define IO_UPDATE(io, M, P) io = (io << 4) | (M << IO_OFF_M) | (P << IO_OFF_P)

// #define TARGET_FPS 60.0
// const float targetFrameTimeMS = 1000.0 / TARGET_FPS;
// #define DT() (float)DWT->CYCCNT * 1000.0 / SystemCoreClock
// #define DTC() DWT->CYCCNT
// #define DT_RESET() DWT->CYCCNT = 0