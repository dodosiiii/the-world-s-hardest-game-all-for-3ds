#ifndef GAME_H
#define GAME_H

#include "level.h"
#include "whg3_world.h"
#include <stdint.h>
#include <stdbool.h>

#define PLAYER_HALF_SIZE 12.5f
#define PLAYER_SPEED_60FPS 2.0f
#define COIN_RADIUS 6.5f
#define KEY_HALF_SIZE 10.5f
#define MAX_DOORS 32
#define MAX_ROOMS 25 // 5x5 grid (A..E x 1..5)
#define MAX_ROOM_COINS 400

enum {
    GAME_EVT_NONE       = 0,
    GAME_EVT_DIE        = 1 << 0,
    GAME_EVT_COIN       = 1 << 1,
    GAME_EVT_KEY        = 1 << 2,
    GAME_EVT_CHECKPOINT = 1 << 3,
    GAME_EVT_WIN        = 1 << 4,
    GAME_EVT_ROOM_WARP  = 1 << 5
};

typedef struct {
    int dx;
    int dy;
} GameInput;

typedef struct {
    const LevelData* lvl;
    uint8_t cur_level; // 1..5
    char cur_col;
    uint8_t cur_row;

    char spawn_col;
    uint8_t spawn_row;
    uint8_t spawn_check_idx;

    char warp_col;
    uint8_t warp_row;
    uint8_t warp_check_idx;

    float player_x;
    float player_y;
    uint8_t player_alpha; // 0..100
    bool dying;

    float enemy_frame;

    // Persistent across rooms in current level
    bool room_coins_collected[MAX_ROOMS][MAX_ROOM_COINS];
    bool room_coins_locked[MAX_ROOMS][MAX_ROOM_COINS];
    bool room_discovered[MAX_ROOMS];
    uint16_t coins_collected_count;

    bool key_collected[MAX_DOORS];
    bool key_locked[MAX_DOORS];
    bool door_open[MAX_DOORS];
    bool gold_door_open;

    // Current room convenience pointers
    bool* coin_collected;

    uint32_t deaths;
    uint32_t level_deaths;
    uint32_t level_ticks;
    bool level_won;
} GameState;

void game_init_level(GameState* gs, uint8_t level_num, const LevelData* first_room, uint32_t initial_deaths);
void game_enter_room(GameState* gs, const LevelData* room, char col, uint8_t row, uint8_t check_idx);
void game_free(GameState* gs);
uint32_t game_update(GameState* gs, GameInput input, float speed_mult);

#endif // GAME_H
