#include "game.h"
#include "physics.h"
#include <stdlib.h>
#include <string.h>

static void find_start_pos(const LevelData* lvl, float* out_x, float* out_y) {
    for (uint16_t i = 0; i < lvl->num_checks; i++) {
        if (lvl->checks[i].kind == 1) {
            *out_x = lvl->checks[i].x + lvl->checks[i].w * 0.5f;
            *out_y = lvl->checks[i].y + lvl->checks[i].h * 0.5f;
            return;
        }
    }
    *out_x = lvl->width * 0.5f;
    *out_y = lvl->height * 0.5f;
}

static void respawn_at_checkpoint(GameState* gs) {
    gs->player_alpha = 100;
    gs->dying = false;
    gs->player_x = gs->spawn_x;
    gs->player_y = gs->spawn_y;
    gs->left_start_check = false;

    // Reset unlocked coins
    for (uint16_t i = 0; i < gs->lvl->num_coins; i++) {
        if (gs->coin_collected[i] && !gs->coin_locked[i]) {
            gs->coin_collected[i] = false;
            if (gs->coins_collected_count > 0) gs->coins_collected_count--;
        }
    }
    // Reset keys and doors
    for (uint16_t i = 0; i < gs->lvl->num_keys; i++) {
        gs->key_collected[i] = false;
    }
    memset(gs->door_open, 0, sizeof(gs->door_open));
}

void game_init(GameState* gs, const LevelData* lvl, uint32_t initial_deaths) {
    memset(gs, 0, sizeof(*gs));
    gs->lvl = lvl;
    gs->deaths = initial_deaths;
    if (lvl->num_coins > 0) {
        gs->coin_collected = (bool*)calloc(lvl->num_coins, sizeof(bool));
        gs->coin_locked = (bool*)calloc(lvl->num_coins, sizeof(bool));
    }
    if (lvl->num_keys > 0) {
        gs->key_collected = (bool*)calloc(lvl->num_keys, sizeof(bool));
    }
    game_restart_level(gs);
}

void game_free(GameState* gs) {
    if (!gs) return;
    free(gs->coin_collected);
    free(gs->coin_locked);
    free(gs->key_collected);
    memset(gs, 0, sizeof(*gs));
}

void game_restart_level(GameState* gs) {
    find_start_pos(gs->lvl, &gs->spawn_x, &gs->spawn_y);
    gs->player_x = gs->spawn_x;
    gs->player_y = gs->spawn_y;
    gs->player_alpha = 100;
    gs->dying = false;
    gs->left_start_check = false;
    gs->enemy_frame = 0.0f;
    gs->level_deaths = 0;
    gs->level_ticks = 0;
    gs->level_won = false;
    gs->coins_collected_count = 0;
    if (gs->coin_collected) memset(gs->coin_collected, 0, gs->lvl->num_coins * sizeof(bool));
    if (gs->coin_locked) memset(gs->coin_locked, 0, gs->lvl->num_coins * sizeof(bool));
    if (gs->key_collected) memset(gs->key_collected, 0, gs->lvl->num_keys * sizeof(bool));
    memset(gs->door_open, 0, sizeof(gs->door_open));
}

static void resolve_walls(const LevelData* lvl, const bool* door_open,
                          float* px, float* py, bool is_x, float delta) {
    for (uint16_t i = 0; i < lvl->num_walls; i++) {
        const Rect16* r = &lvl->walls[i];
        if (is_x) {
            *px = phys_resolve_axis(*px, *py, PLAYER_HALF_SIZE,
                                    (float)r->x, (float)r->y, (float)r->w, (float)r->h, true, delta);
        } else {
            *py = phys_resolve_axis(*py, *px, PLAYER_HALF_SIZE,
                                    (float)r->x, (float)r->y, (float)r->w, (float)r->h, false, delta);
        }
    }
    for (uint16_t i = 0; i < lvl->num_doors; i++) {
        const DoorRect* d = &lvl->doors[i];
        if (d->door_id < MAX_DOORS && door_open[d->door_id]) continue;
        if (is_x) {
            *px = phys_resolve_axis(*px, *py, PLAYER_HALF_SIZE,
                                    (float)d->x, (float)d->y, (float)d->w, (float)d->h, true, delta);
        } else {
            *py = phys_resolve_axis(*py, *px, PLAYER_HALF_SIZE,
                                    (float)d->x, (float)d->y, (float)d->w, (float)d->h, false, delta);
        }
    }
}

uint32_t game_update(GameState* gs, GameInput input) {
    uint32_t events = GAME_EVT_NONE;
    if (gs->level_won) return events;

    const LevelData* lvl = gs->lvl;
    gs->level_ticks++;
    gs->enemy_frame += 0.5f; // 30 FPS timeline at 60 FPS game loop

    if (gs->dying) {
        if (gs->player_alpha > 2) {
            gs->player_alpha -= 2; // 50 ticks = 0.83s at 60 FPS
        } else {
            gs->deaths++;
            gs->level_deaths++;
            respawn_at_checkpoint(gs);
        }
        return events;
    }

    // Move X then resolve walls
    float move_x = (float)input.dx * PLAYER_SPEED_60FPS;
    if (move_x != 0.0f) {
        gs->player_x += move_x;
        resolve_walls(lvl, gs->door_open, &gs->player_x, &gs->player_y, true, move_x);
    }

    // Move Y then resolve walls
    float move_y = (float)input.dy * PLAYER_SPEED_60FPS;
    if (move_y != 0.0f) {
        gs->player_y += move_y;
        resolve_walls(lvl, gs->door_open, &gs->player_x, &gs->player_y, false, move_y);
    }

    // Clamp to stage bounds
    if (gs->player_x < PLAYER_HALF_SIZE) gs->player_x = PLAYER_HALF_SIZE;
    if (gs->player_x > lvl->width - PLAYER_HALF_SIZE) gs->player_x = lvl->width - PLAYER_HALF_SIZE;
    if (gs->player_y < PLAYER_HALF_SIZE) gs->player_y = PLAYER_HALF_SIZE;
    if (gs->player_y > lvl->height - PLAYER_HALF_SIZE) gs->player_y = lvl->height - PLAYER_HALF_SIZE;

    // Collect coins
    for (uint16_t i = 0; i < lvl->num_coins; i++) {
        if (gs->coin_collected[i]) continue;
        if (phys_square_circle_overlap(gs->player_x, gs->player_y, PLAYER_HALF_SIZE,
                                       (float)lvl->coins[i].x, (float)lvl->coins[i].y, COIN_RADIUS)) {
            gs->coin_collected[i] = true;
            gs->coins_collected_count++;
            events |= GAME_EVT_COIN;
        }
    }

    // Collect keys
    for (uint16_t i = 0; i < lvl->num_keys; i++) {
        if (gs->key_collected[i]) continue;
        if (phys_rect_overlap(gs->player_x - PLAYER_HALF_SIZE, gs->player_y - PLAYER_HALF_SIZE,
                              PLAYER_HALF_SIZE * 2.0f, PLAYER_HALF_SIZE * 2.0f,
                              (float)lvl->keys[i].x - KEY_HALF_SIZE, (float)lvl->keys[i].y - KEY_HALF_SIZE,
                              KEY_HALF_SIZE * 2.0f, KEY_HALF_SIZE * 2.0f)) {
            gs->key_collected[i] = true;
            if (lvl->keys[i].door_id < MAX_DOORS) {
                gs->door_open[lvl->keys[i].door_id] = true;
            }
            events |= GAME_EVT_KEY;
        }
    }

    // Checkpoints & Goal
    bool has_check2 = false;
    for (uint16_t i = 0; i < lvl->num_checks; i++) {
        if (lvl->checks[i].kind == 2) { has_check2 = true; break; }
    }

    for (uint16_t i = 0; i < lvl->num_checks; i++) {
        const CheckRect* c = &lvl->checks[i];
        bool inside = phys_rect_overlap(
            gs->player_x - PLAYER_HALF_SIZE, gs->player_y - PLAYER_HALF_SIZE,
            PLAYER_HALF_SIZE * 2.0f, PLAYER_HALF_SIZE * 2.0f,
            (float)c->x, (float)c->y, (float)c->w, (float)c->h);

        if (c->kind == 1 && !inside) {
            gs->left_start_check = true;
        }

        if (!inside) continue;

        if (c->kind == 2 || (c->kind == 1 && !has_check2 && gs->left_start_check)) {
            if (gs->coins_collected_count >= lvl->num_coins) {
                gs->level_won = true;
                events |= GAME_EVT_WIN;
                return events;
            }
        } else if (c->kind == 3) {
            float cx = c->x + c->w * 0.5f;
            float cy = c->y + c->h * 0.5f;
            if (gs->spawn_x != cx || gs->spawn_y != cy) {
                gs->spawn_x = cx;
                gs->spawn_y = cy;
                for (uint16_t k = 0; k < lvl->num_coins; k++) {
                    if (gs->coin_collected[k]) gs->coin_locked[k] = true;
                }
                events |= GAME_EVT_CHECKPOINT;
            }
        }
    }

    // Enemy collisions
    for (uint16_t i = 0; i < lvl->num_enemies; i++) {
        float ex = 0.0f, ey = 0.0f;
        level_get_enemy_pos(&lvl->enemies[i], gs->enemy_frame, &ex, &ey);
        if (phys_square_circle_overlap(gs->player_x, gs->player_y, PLAYER_HALF_SIZE,
                                       ex, ey, lvl->enemies[i].radius)) {
            gs->dying = true;
            events |= GAME_EVT_DIE;
            break;
        }
    }

    return events;
}
