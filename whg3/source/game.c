#include "game.h"
#include "physics.h"
#include <stdlib.h>
#include <string.h>

static int room_index(char col, uint8_t row) {
    int c = (col >= 'A' && col <= 'E') ? (col - 'A') : 0;
    int r = (row >= 1 && row <= 5) ? (row - 1) : 0;
    return c * 5 + r;
}

static void place_player_at_check(GameState* gs, uint8_t check_idx) {
    const LevelData* lvl = gs->lvl;
    for (uint16_t i = 0; i < lvl->num_checks; i++) {
        if (lvl->checks[i].kind == check_idx) {
            gs->player_x = lvl->checks[i].x + lvl->checks[i].w * 0.5f;
            gs->player_y = lvl->checks[i].y + lvl->checks[i].h * 0.5f;
            if (gs->player_x >= 877.0f) gs->player_x -= 6.0f;
            else if (gs->player_x <= 34.0f) gs->player_x += 5.0f;
            return;
        }
    }
    gs->player_x = lvl->width * 0.5f;
    gs->player_y = lvl->height * 0.5f;
}

void game_enter_room(GameState* gs, const LevelData* room, char col, uint8_t row, uint8_t check_idx) {
    gs->lvl = room;
    gs->cur_col = col;
    gs->cur_row = row;
    int ridx = room_index(col, row);
    gs->room_discovered[ridx] = true;
    gs->coin_collected = gs->room_coins_collected[ridx];
    gs->enemy_frame = 0.0f;
    gs->player_alpha = 100;
    gs->dying = false;
    place_player_at_check(gs, check_idx);
}

void game_init_level(GameState* gs, uint8_t level_num, const LevelData* first_room, uint32_t initial_deaths) {
    memset(gs, 0, sizeof(*gs));
    gs->cur_level = level_num;
    gs->deaths = initial_deaths;
    const Whg3StartFinish* st = &WHG3_STARTS[level_num - 1];
    gs->spawn_col = st->col;
    gs->spawn_row = st->row;
    gs->spawn_check_idx = st->check_idx;
    game_enter_room(gs, first_room, st->col, st->row, st->check_idx);
}

void game_free(GameState* gs) {
    if (!gs) return;
    memset(gs, 0, sizeof(*gs));
}

static void reset_unlocked_on_death(GameState* gs) {
    for (int r = 0; r < MAX_ROOMS; r++) {
        for (int c = 0; c < MAX_ROOM_COINS; c++) {
            if (gs->room_coins_collected[r][c] && !gs->room_coins_locked[r][c]) {
                gs->room_coins_collected[r][c] = false;
                if (gs->coins_collected_count > 0) gs->coins_collected_count--;
            }
        }
    }
    for (int k = 0; k < MAX_DOORS; k++) {
        if (gs->key_collected[k] && !gs->key_locked[k]) {
            gs->key_collected[k] = false;
            gs->door_open[k] = false;
        }
    }
    gs->gold_door_open = (gs->coins_collected_count >= WHG3_LEVEL_COINS[gs->cur_level - 1]);
}

static void resolve_walls(const GameState* gs, float* px, float* py, bool is_x, float delta) {
    const LevelData* lvl = gs->lvl;
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
        if (d->door_id == 255 && gs->gold_door_open) continue;
        if (d->door_id < MAX_DOORS && gs->door_open[d->door_id]) continue;
        if (is_x) {
            *px = phys_resolve_axis(*px, *py, PLAYER_HALF_SIZE,
                                    (float)d->x, (float)d->y, (float)d->w, (float)d->h, true, delta);
        } else {
            *py = phys_resolve_axis(*py, *px, PLAYER_HALF_SIZE,
                                    (float)d->x, (float)d->y, (float)d->w, (float)d->h, false, delta);
        }
    }
}

uint32_t game_update(GameState* gs, GameInput input, float speed_mult) {
    uint32_t events = GAME_EVT_NONE;
    if (gs->level_won) return events;

    const LevelData* lvl = gs->lvl;
    gs->level_ticks++;
    gs->enemy_frame += 0.5f;

    if (gs->dying) {
        if (gs->player_alpha > 2) {
            gs->player_alpha -= 2;
        } else {
            gs->deaths++;
            gs->level_deaths++;
            reset_unlocked_on_death(gs);
            gs->warp_col = gs->spawn_col;
            gs->warp_row = gs->spawn_row;
            gs->warp_check_idx = gs->spawn_check_idx;
            events |= GAME_EVT_ROOM_WARP;
        }
        return events;
    }

    float spd = PLAYER_SPEED_60FPS * speed_mult;
    float move_x = (float)input.dx * spd;
    if (move_x != 0.0f) {
        gs->player_x += move_x;
        resolve_walls(gs, &gs->player_x, &gs->player_y, true, move_x);
    }

    float move_y = (float)input.dy * spd;
    if (move_y != 0.0f) {
        gs->player_y += move_y;
        resolve_walls(gs, &gs->player_x, &gs->player_y, false, move_y);
    }

    if (gs->player_x < PLAYER_HALF_SIZE) gs->player_x = PLAYER_HALF_SIZE;
    if (gs->player_x > lvl->width - PLAYER_HALF_SIZE) gs->player_x = lvl->width - PLAYER_HALF_SIZE;
    if (gs->player_y < PLAYER_HALF_SIZE) gs->player_y = PLAYER_HALF_SIZE;
    if (gs->player_y > lvl->height - PLAYER_HALF_SIZE) gs->player_y = lvl->height - PLAYER_HALF_SIZE;

    int ridx = room_index(gs->cur_col, gs->cur_row);
    uint16_t max_c = lvl->num_coins < MAX_ROOM_COINS ? lvl->num_coins : MAX_ROOM_COINS;
    for (uint16_t i = 0; i < max_c; i++) {
        if (gs->room_coins_collected[ridx][i]) continue;
        if (phys_square_circle_overlap(gs->player_x, gs->player_y, PLAYER_HALF_SIZE,
                                       (float)lvl->coins[i].x, (float)lvl->coins[i].y, COIN_RADIUS)) {
            gs->room_coins_collected[ridx][i] = true;
            gs->coins_collected_count++;
            if (gs->coins_collected_count >= WHG3_LEVEL_COINS[gs->cur_level - 1]) {
                gs->gold_door_open = true;
            }
            events |= GAME_EVT_COIN;
        }
    }

    for (uint16_t i = 0; i < lvl->num_keys; i++) {
        uint16_t kid = lvl->keys[i].door_id;
        if (kid < MAX_DOORS && gs->key_collected[kid]) continue;
        if (phys_rect_overlap(gs->player_x - PLAYER_HALF_SIZE, gs->player_y - PLAYER_HALF_SIZE,
                              PLAYER_HALF_SIZE * 2.0f, PLAYER_HALF_SIZE * 2.0f,
                              (float)lvl->keys[i].x - KEY_HALF_SIZE, (float)lvl->keys[i].y - KEY_HALF_SIZE,
                              KEY_HALF_SIZE * 2.0f, KEY_HALF_SIZE * 2.0f)) {
            if (kid < MAX_DOORS) {
                gs->key_collected[kid] = true;
                gs->door_open[kid] = true;
            }
            events |= GAME_EVT_KEY;
        }
    }

    const Whg3StartFinish* fin = &WHG3_FINISHES[gs->cur_level - 1];
    for (uint16_t i = 0; i < lvl->num_checks; i++) {
        const CheckRect* c = &lvl->checks[i];
        if (!phys_rect_overlap(gs->player_x - PLAYER_HALF_SIZE, gs->player_y - PLAYER_HALF_SIZE,
                               PLAYER_HALF_SIZE * 2.0f, PLAYER_HALF_SIZE * 2.0f,
                               (float)c->x, (float)c->y, (float)c->w, (float)c->h)) {
            continue;
        }

        // Check finish
        if (gs->cur_col == fin->col && gs->cur_row == fin->row && c->kind == fin->check_idx) {
            if (gs->coins_collected_count >= WHG3_LEVEL_COINS[gs->cur_level - 1]) {
                gs->level_won = true;
                events |= GAME_EVT_WIN;
                return events;
            }
        }

        // Check room entrance/warp
        bool is_warp = false;
        for (size_t e = 0; e < WHG3_NUM_ENTRANCES; e++) {
            const Whg3Entrance* ent = &WHG3_ENTRANCES[e];
            if (ent->level == gs->cur_level && ent->col == gs->cur_col &&
                ent->row == gs->cur_row && ent->check_idx == c->kind) {
                gs->warp_col = ent->dst_col;
                gs->warp_row = ent->dst_row;
                gs->warp_check_idx = ent->dst_check_idx;
                events |= GAME_EVT_ROOM_WARP;
                is_warp = true;
                break;
            }
        }
        if (is_warp) return events;

        // Otherwise it's a checkpoint: save spawn & lock coins/keys
        if (gs->spawn_col != gs->cur_col || gs->spawn_row != gs->cur_row || gs->spawn_check_idx != c->kind) {
            gs->spawn_col = gs->cur_col;
            gs->spawn_row = gs->cur_row;
            gs->spawn_check_idx = (uint8_t)c->kind;
            for (int r = 0; r < MAX_ROOMS; r++) {
                for (int ci = 0; ci < MAX_ROOM_COINS; ci++) {
                    if (gs->room_coins_collected[r][ci]) gs->room_coins_locked[r][ci] = true;
                }
            }
            for (int k = 0; k < MAX_DOORS; k++) {
                if (gs->key_collected[k]) gs->key_locked[k] = true;
            }
            events |= GAME_EVT_CHECKPOINT;
        }
    }

    for (uint16_t i = 0; i < lvl->num_enemies; i++) {
        float ex = 0, ey = 0;
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
