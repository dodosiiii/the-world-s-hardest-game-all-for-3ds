#ifndef GAME_H
#define GAME_H

#include "level.h"
#include <stdint.h>
#include <stdbool.h>

#define PLAYER_HALF_SIZE 9.0f
#define PLAYER_SPEED_60FPS 1.5f
#define COIN_RADIUS 6.5f
#define KEY_HALF_SIZE 10.5f
#define MAX_DOORS 8

enum {
    GAME_EVT_NONE       = 0,
    GAME_EVT_DIE        = 1 << 0,
    GAME_EVT_COIN       = 1 << 1,
    GAME_EVT_KEY        = 1 << 2,
    GAME_EVT_CHECKPOINT = 1 << 3,
    GAME_EVT_WIN        = 1 << 4
};

typedef struct {
    int dx; // -1, 0, +1
    int dy; // -1, 0, +1
} GameInput;

typedef struct {
    const LevelData* lvl;

    float player_x;
    float player_y;
    float spawn_x;
    float spawn_y;
    uint8_t player_alpha; // 0..100
    bool dying;
    bool left_start_check; // for single-check levels where check1 is also goal

    float enemy_frame; // 30 FPS timeline cursor (advances +0.5 per 60 FPS tick)

    bool* coin_collected;
    bool* coin_locked;
    uint16_t coins_collected_count;

    bool* key_collected;
    bool door_open[MAX_DOORS];

    uint32_t deaths;
    uint32_t level_deaths;
    uint32_t level_ticks;
    bool level_won;
} GameState;

void game_init(GameState* gs, const LevelData* lvl, uint32_t initial_deaths);
void game_free(GameState* gs);
void game_restart_level(GameState* gs);
uint32_t game_update(GameState* gs, GameInput input);

#endif // GAME_H
