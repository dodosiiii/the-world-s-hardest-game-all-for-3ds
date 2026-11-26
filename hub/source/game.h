#ifndef GAME_H
#define GAME_H

#include "level.h"
#include "whg3_world.h"
#include "whg4_world.h"
#include <stdint.h>
#include <stdbool.h>

#define COIN_RADIUS 6.5f
#define DIAMOND_RADIUS 8.5f
#define KEY_HALF_SIZE 10.5f
#define WATER_MAX_TICKS 600
#define MAX_DOORS 64
#define MAX_ROOMS 25
#define MAX_ROOM_COINS 450
#define MAX_ROOM_DIAMONDS 32

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
    bool is_whg3;
    bool is_whg4;
    float base_speed;
    float player_half_size;
    float player_draw_half_size;

    // WHG3 multi-room state
    uint8_t cur_level;
    char cur_col;
    uint8_t cur_row;
    char spawn_col;
    uint8_t spawn_row;
    uint8_t spawn_check_idx;
    uint8_t room_entry_check_idx;
    bool left_room_entry_check;
    char warp_col;
    uint8_t warp_row;
    uint8_t warp_check_idx;
    bool warp_place_at_spawn;

    float player_x;
    float player_y;
    float spawn_x;
    float spawn_y;
    uint8_t player_alpha; // 0..100
    bool dying;
    bool left_start_check;
    bool left_spawn_check;

    float enemy_frame;

    bool* coin_collected;
    bool* coin_locked;
    uint16_t coins_collected_count;

    bool* key_collected;
    bool door_open[MAX_DOORS];
    bool gold_door_open;

    // WHG3/4 persistent room coins, diamonds & cross-room keys
    bool room_coins_collected[MAX_ROOMS][MAX_ROOM_COINS];
    bool room_coins_locked[MAX_ROOMS][MAX_ROOM_COINS];
    bool room_diamonds_collected[MAX_ROOMS][MAX_ROOM_DIAMONDS];
    bool room_diamonds_locked[MAX_ROOMS][MAX_ROOM_DIAMONDS];
    bool* diamond_collected;
    uint16_t diamonds_collected_count;
    bool whg3_key_collected[MAX_DOORS];
    bool whg3_key_locked[MAX_DOORS];

    // WHG4 terrain / water / ice state
    float ice_vx;
    float ice_vy;
    uint16_t water_ticks;
    bool in_water;

    uint32_t deaths;
    uint32_t level_deaths;
    uint32_t level_ticks;
    bool level_won;
    bool invincible;
} GameState;

bool game_init(GameState* gs, const LevelData* lvl, uint32_t initial_deaths);
void game_init_whg3(GameState* gs, uint8_t level_num, const LevelData* first_room, uint32_t initial_deaths);
void game_init_whg3_at(GameState* gs, uint8_t level_num, const LevelData* room, char col, uint8_t row, uint32_t initial_deaths);
void game_enter_room_whg3(GameState* gs, const LevelData* room, char col, uint8_t row, uint8_t check_idx);
void game_init_whg4(GameState* gs, uint8_t level_num, const LevelData* first_room, uint32_t initial_deaths);
void game_init_whg4_at(GameState* gs, uint8_t level_num, const LevelData* room, char col, uint8_t row, uint32_t initial_deaths);
void game_enter_room_whg4(GameState* gs, const LevelData* room, char col, uint8_t row, bool place_at_spawn, uint8_t check_idx);
uint16_t game_total_cash(const GameState* gs);
void game_free(GameState* gs);
void game_restart_level(GameState* gs);
uint32_t game_update(GameState* gs, GameInput input, float speed_mult);

#endif // GAME_H
