#ifndef WHG4_WORLD_H
#define WHG4_WORLD_H

#include <stdint.h>

typedef struct {
    char col;
    uint8_t row;
    uint8_t check_idx;
} Whg4StartFinish;

static const Whg4StartFinish WHG4_STARTS[5] = {
    {'B', 4, 0},
    {'B', 2, 0},
    {'B', 1, 0},
    {'A', 1, 0},
    {'A', 3, 0},
};

static const Whg4StartFinish WHG4_FINISHES[5] = {
    {'A', 1, 1},
    {'A', 2, 2},
    {'C', 1, 0},
    {'D', 3, 1},
    {'C', 1, 0},
};

// Total cash per level (coins * 1 + diamonds * 100)
static const uint16_t WHG4_LEVEL_CASH[5] = { 276, 171, 1718, 1630, 2705 };
#define WHG4_L5_BOSS_CASH 2296

#endif // WHG4_WORLD_H
