#ifndef GAME_H
#define GAME_H

#include "level.h"
#include "whg4_world.h"
#include <stdint.h>
#include <stdbool.h>

#define PLAYER_HALF_SIZE 11.0f
#define PLAYER_SPEED_60FPS 2.0f
#define WATER_SPEED_60FPS 1.0f
#define ICE_SPEED_60FPS 3.0f
#define WATER_MAX_TICKS 600 // 10 seconds @60fps
#define COIN_RADIUS 6.5f
#define DIAMOND_RADIUS 8.5f
#define KEY_HALF_SIZE 10.5f
#define MAX_DOORS 64
#define MAX_ROOMS 25 // 5x5 grid (A..E x 1..5)
#define MAX_ROOM_COINS 450
#define MAX_ROOM_DIAMONDS 32
#define MAX_ROOM_MPS 40

enum {
    GAME_EVT_NONE       = 0,
    GAME_EVT_DIE        = 1 << 0,
    GAME_EVT_COIN       = 1 << 1,
    GAME_EVT_KEY        = 1 << 2,
    GAME_EVT_CHECKPOINT = 1 << 3,
    GAME_EVT_WIN        = 1 << 4,
    GAME_EVT_ROOM_WARP  = 1 << 5,
    GAME_EVT_DIAMOND    = 1 << 6,
    GAME_EVT_DIE_WATER  = 1 << 7,
    GAME_EVT_DIE_PIT    = 1 << 8
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
    bool warp_place_at_spawn;

    float player_x;
    float player_y;
    float ice_vx;
    float ice_vy;
    uint16_t water_ticks;
    bool in_water;
    uint8_t player_alpha; // 0..100
    bool dying;

    float enemy_frame;

    // Persistent across rooms in current level
    bool room_coins_collected[MAX_ROOMS][MAX_ROOM_COINS];
    bool room_coins_locked[MAX_ROOMS][MAX_ROOM_COINS];
    bool room_diamonds_collected[MAX_ROOMS][MAX_ROOM_DIAMONDS];
    bool room_diamonds_locked[MAX_ROOMS][MAX_ROOM_DIAMONDS];
    bool room_discovered[MAX_ROOMS];
    uint16_t coins_collected_count;
    uint16_t diamonds_collected_count;

    bool key_collected[MAX_DOORS];
    bool key_locked[MAX_DOORS];
    bool door_open[MAX_DOORS];
    bool gold_door_open;

    // Current room convenience pointers
    bool* coin_collected;
    bool* diamond_collected;

    uint32_t deaths;
    uint32_t level_deaths;
    uint32_t level_ticks;
    bool level_won;
} GameState;

void game_init_level(GameState* gs, uint8_t level_num, const LevelData* first_room, uint32_t initial_deaths);
void game_enter_room(GameState* gs, const LevelData* room, char col, uint8_t row, bool place_at_spawn, uint8_t check_idx);
void game_free(GameState* gs);
uint32_t game_update(GameState* gs, GameInput input, float speed_mult);
uint16_t game_total_cash(const GameState* gs);

#endif // GAME_H
