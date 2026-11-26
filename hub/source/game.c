#include "game.h"
#include "physics.h"
#include <stdlib.h>
#include <string.h>

static int room_index(char col, uint8_t row) {
    int c = (col >= 'A' && col <= 'E') ? (col - 'A') : 0;
    int r = (row >= 1 && row <= 5) ? (row - 1) : 0;
    return c * 5 + r;
}

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

static void place_player_at_check_whg3(GameState* gs, uint8_t check_idx) {
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

static void respawn_at_checkpoint(GameState* gs) {
    gs->player_alpha = 100;
    gs->dying = false;
    gs->player_x = gs->spawn_x;
    gs->player_y = gs->spawn_y;
    gs->left_start_check = false;

    for (uint16_t i = 0; i < gs->lvl->num_coins; i++) {
        if (gs->coin_collected[i] && !gs->coin_locked[i]) {
            gs->coin_collected[i] = false;
            if (gs->coins_collected_count > 0) gs->coins_collected_count--;
        }
    }
    for (uint16_t i = 0; i < gs->lvl->num_keys; i++) {
        gs->key_collected[i] = false;
    }
    memset(gs->door_open, 0, sizeof(gs->door_open));
}

static void reset_unlocked_whg3(GameState* gs) {
    for (int r = 0; r < MAX_ROOMS; r++) {
        for (int c = 0; c < MAX_ROOM_COINS; c++) {
            if (gs->room_coins_collected[r][c] && !gs->room_coins_locked[r][c]) {
                gs->room_coins_collected[r][c] = false;
                if (gs->coins_collected_count > 0) gs->coins_collected_count--;
            }
        }
        for (int d = 0; d < MAX_ROOM_DIAMONDS; d++) {
            if (gs->room_diamonds_collected[r][d] && !gs->room_diamonds_locked[r][d]) {
                gs->room_diamonds_collected[r][d] = false;
                if (gs->diamonds_collected_count > 0) gs->diamonds_collected_count--;
            }
        }
    }
    for (int k = 0; k < MAX_DOORS; k++) {
        if (gs->whg3_key_collected[k] && !gs->whg3_key_locked[k]) {
            gs->whg3_key_collected[k] = false;
            gs->door_open[k] = false;
        }
    }
    if (gs->is_whg4) {
        uint16_t cash = game_total_cash(gs);
        uint16_t tgt = (gs->cur_level == 5 && gs->cur_col == 'C' && gs->cur_row == 2)
                       ? WHG4_L5_BOSS_CASH : WHG4_LEVEL_CASH[gs->cur_level - 1];
        gs->gold_door_open = (cash >= tgt);
    } else {
        gs->gold_door_open = (gs->coins_collected_count >= WHG3_LEVEL_COINS[gs->cur_level - 1]);
    }
}

uint16_t game_total_cash(const GameState* gs) {
    return gs->coins_collected_count + gs->diamonds_collected_count * 100;
}

static void update_gold_door_whg4(GameState* gs) {
    uint16_t cash = game_total_cash(gs);
    uint16_t tgt = (gs->cur_level == 5 && gs->cur_col == 'C' && gs->cur_row == 2)
                   ? WHG4_L5_BOSS_CASH : WHG4_LEVEL_CASH[gs->cur_level - 1];
    gs->gold_door_open = (cash >= tgt);
}

bool game_init(GameState* gs, const LevelData* lvl, uint32_t initial_deaths) {
    if (!gs || !lvl) return false;
    memset(gs, 0, sizeof(*gs));
    gs->lvl = lvl;
    gs->is_whg3 = false;
    gs->base_speed = 1.5f;
    gs->player_half_size = 9.0f;
    gs->player_draw_half_size = 9.0f;
    gs->deaths = initial_deaths;
    if (lvl->num_coins > 0) {
        gs->coin_collected = (bool*)calloc(lvl->num_coins, sizeof(bool));
        gs->coin_locked = (bool*)calloc(lvl->num_coins, sizeof(bool));
    }
    if (lvl->num_keys > 0) {
        gs->key_collected = (bool*)calloc(lvl->num_keys, sizeof(bool));
    }
    if ((lvl->num_coins > 0 && (!gs->coin_collected || !gs->coin_locked)) ||
        (lvl->num_keys > 0 && !gs->key_collected)) {
        game_free(gs);
        return false;
    }
    game_restart_level(gs);
    return true;
}

void game_enter_room_whg3(GameState* gs, const LevelData* room, char col, uint8_t row, uint8_t check_idx) {
    gs->lvl = room;
    gs->cur_col = col;
    gs->cur_row = row;
    int ridx = room_index(col, row);
    gs->coin_collected = gs->room_coins_collected[ridx];
    gs->enemy_frame = 0.0f;
    gs->player_alpha = 100;
    gs->dying = false;
    gs->left_spawn_check = false;
    gs->room_entry_check_idx = check_idx;
    gs->left_room_entry_check = false;
    place_player_at_check_whg3(gs, check_idx);
}

void game_init_whg3(GameState* gs, uint8_t level_num, const LevelData* first_room, uint32_t initial_deaths) {
    const Whg3StartFinish* st = &WHG3_STARTS[level_num - 1];
    game_init_whg3_at(gs, level_num, first_room, st->col, st->row, initial_deaths);
}

void game_init_whg3_at(GameState* gs, uint8_t level_num, const LevelData* room, char col, uint8_t row, uint32_t initial_deaths) {
    memset(gs, 0, sizeof(*gs));
    gs->is_whg3 = true;
    gs->base_speed = 2.0f;
    gs->player_half_size = 9.0f;
    gs->player_draw_half_size = 12.5f;
    gs->cur_level = level_num;
    gs->deaths = initial_deaths;
    const Whg3StartFinish* st = &WHG3_STARTS[level_num - 1];
    uint8_t cidx = (col == st->col && row == st->row) ? st->check_idx
                 : (room->num_checks > 0 ? (uint8_t)room->checks[0].kind : 0);
    gs->spawn_col = col;
    gs->spawn_row = row;
    gs->spawn_check_idx = cidx;
    game_enter_room_whg3(gs, room, col, row, cidx);
}

void game_enter_room_whg4(GameState* gs, const LevelData* room, char col, uint8_t row, bool place_at_spawn, uint8_t check_idx) {
    gs->lvl = room;
    gs->cur_col = col;
    gs->cur_row = row;
    int ridx = room_index(col, row);
    gs->coin_collected = gs->room_coins_collected[ridx];
    gs->diamond_collected = gs->room_diamonds_collected[ridx];
    gs->enemy_frame = 0.0f;
    gs->player_alpha = 100;
    gs->dying = false;
    gs->left_spawn_check = false;
    gs->ice_vx = 0.0f;
    gs->ice_vy = 0.0f;
    update_gold_door_whg4(gs);
    if (place_at_spawn) {
        for (uint16_t i = 0; i < room->num_checks; i++) {
            if (room->checks[i].kind == check_idx) {
                gs->player_x = room->checks[i].x + room->checks[i].w * 0.5f;
                gs->player_y = room->checks[i].y + room->checks[i].h * 0.5f;
                return;
            }
        }
        gs->player_x = room->width * 0.5f;
        gs->player_y = room->height * 0.5f;
    }
}

void game_init_whg4(GameState* gs, uint8_t level_num, const LevelData* first_room, uint32_t initial_deaths) {
    const Whg4StartFinish* st = &WHG4_STARTS[level_num - 1];
    game_init_whg4_at(gs, level_num, first_room, st->col, st->row, initial_deaths);
}

void game_init_whg4_at(GameState* gs, uint8_t level_num, const LevelData* room, char col, uint8_t row, uint32_t initial_deaths) {
    memset(gs, 0, sizeof(*gs));
    gs->is_whg4 = true;
    gs->base_speed = 2.0f;
    gs->player_half_size = 9.0f;
    gs->player_draw_half_size = 11.0f;
    gs->cur_level = level_num;
    gs->deaths = initial_deaths;
    gs->water_ticks = WATER_MAX_TICKS;
    const Whg4StartFinish* st = &WHG4_STARTS[level_num - 1];
    uint8_t cidx = (col == st->col && row == st->row) ? st->check_idx
                 : (room->num_checks > 0 ? (uint8_t)room->checks[0].kind : 0);
    gs->spawn_col = col;
    gs->spawn_row = row;
    gs->spawn_check_idx = cidx;
    game_enter_room_whg4(gs, room, col, row, true, cidx);
}

void game_free(GameState* gs) {
    if (!gs) return;
    if (!gs->is_whg3 && !gs->is_whg4) {
        free(gs->coin_collected);
        free(gs->coin_locked);
        free(gs->key_collected);
    }
    memset(gs, 0, sizeof(*gs));
}

void game_restart_level(GameState* gs) {
    if (gs->is_whg3 || gs->is_whg4) return;
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

static void resolve_walls(const GameState* gs, float* px, float* py, bool is_x, float delta) {
    const LevelData* lvl = gs->lvl;
    float hs = gs->player_half_size;
    for (uint16_t i = 0; i < lvl->num_walls; i++) {
        const Rect16* r = &lvl->walls[i];
        if (is_x) {
            *px = phys_resolve_axis(*px, *py, hs, (float)r->x, (float)r->y, (float)r->w, (float)r->h, true, delta);
        } else {
            *py = phys_resolve_axis(*py, *px, hs, (float)r->x, (float)r->y, (float)r->w, (float)r->h, false, delta);
        }
    }
    for (uint16_t i = 0; i < lvl->num_doors; i++) {
        const DoorRect* d = &lvl->doors[i];
        if ((gs->is_whg3 || gs->is_whg4) && d->door_id == 255 && gs->gold_door_open) continue;
        if (d->door_id < MAX_DOORS && gs->door_open[d->door_id]) continue;
        if (is_x) {
            *px = phys_resolve_axis(*px, *py, hs, (float)d->x, (float)d->y, (float)d->w, (float)d->h, true, delta);
        } else {
            *py = phys_resolve_axis(*py, *px, hs, (float)d->x, (float)d->y, (float)d->w, (float)d->h, false, delta);
        }
    }
}

static uint8_t get_terrain_at(const LevelData* lvl, float x, float y) {
    for (uint16_t i = 0; i < lvl->num_terrains; i++) {
        const TerrainRect* t = &lvl->terrains[i];
        if (x >= t->x && x < t->x + t->w && y >= t->y && y < t->y + t->h) {
            return t->type;
        }
    }
    return TERR_NONE;
}

static int find_mp_at(const LevelData* lvl, float anim_frame, float px, float py) {
    for (uint16_t i = 0; i < lvl->num_mps; i++) {
        float mx, my;
        level_get_mp_pos(&lvl->mps[i], anim_frame, &mx, &my);
        if (px >= mx && px <= mx + lvl->mps[i].w &&
            py >= my && py <= my + lvl->mps[i].h) {
            return (int)i;
        }
    }
    return -1;
}

static uint32_t game_update_whg4(GameState* gs, GameInput input, float speed_mult) {
    uint32_t evt = GAME_EVT_NONE;
    const LevelData* lvl = gs->lvl;
    if (!lvl || gs->level_won) return evt;
    if (gs->invincible && gs->dying) {
        gs->dying = false;
        gs->player_alpha = 100;
    }

    float hs = gs->player_half_size;
    gs->level_ticks++;
    float prev_frame = gs->enemy_frame;
    gs->enemy_frame += 0.5f;

    if (gs->dying) {
        if (gs->player_alpha > 5) {
            gs->player_alpha -= 5;
        } else {
            gs->player_alpha = 100;
            gs->dying = false;
            gs->ice_vx = 0.0f;
            gs->ice_vy = 0.0f;
            gs->water_ticks = WATER_MAX_TICKS;
            gs->in_water = false;
            gs->deaths++;
            gs->level_deaths++;
            reset_unlocked_whg3(gs);
            gs->warp_col = gs->spawn_col;
            gs->warp_row = gs->spawn_row;
            gs->warp_check_idx = gs->spawn_check_idx;
            gs->warp_place_at_spawn = true;
            return evt | GAME_EVT_ROOM_WARP;
        }
        return evt;
    }

    int mp_idx = find_mp_at(lvl, prev_frame, gs->player_x, gs->player_y);
    if (mp_idx >= 0) {
        float x0, y0, x1, y1;
        level_get_mp_pos(&lvl->mps[mp_idx], prev_frame, &x0, &y0);
        level_get_mp_pos(&lvl->mps[mp_idx], gs->enemy_frame, &x1, &y1);
        float mdx = x1 - x0, mdy = y1 - y0;
        if (mdx != 0.0f) { gs->player_x += mdx; resolve_walls(gs, &gs->player_x, &gs->player_y, true, mdx); }
        if (mdy != 0.0f) { gs->player_y += mdy; resolve_walls(gs, &gs->player_x, &gs->player_y, false, mdy); }
    }

    uint8_t terr = get_terrain_at(lvl, gs->player_x, gs->player_y);
    float cbx = 0.0f, cby = 0.0f;
    if (terr == TERR_CBR1) cbx = 0.5f;
    else if (terr == TERR_CBR2) cbx = 1.0f;
    else if (terr == TERR_CBR3) cbx = 2.0f;
    else if (terr == TERR_CBL1) cbx = -0.5f;
    else if (terr == TERR_CBL2) cbx = -1.0f;
    else if (terr == TERR_CBL3) cbx = -2.0f;
    else if (terr == TERR_CBU1) cby = -0.5f;
    else if (terr == TERR_CBU2) cby = -1.0f;
    else if (terr == TERR_CBU3) cby = -2.0f;
    else if (terr == TERR_CBD1) cby = 0.5f;
    else if (terr == TERR_CBD2) cby = 1.0f;
    else if (terr == TERR_CBD3) cby = 2.0f;
    if (cbx != 0.0f) { gs->player_x += cbx; resolve_walls(gs, &gs->player_x, &gs->player_y, true, cbx); }
    if (cby != 0.0f) { gs->player_y += cby; resolve_walls(gs, &gs->player_x, &gs->player_y, false, cby); }

    float dx = 0.0f, dy = 0.0f;
    if (terr == TERR_ICE) {
        float max_ice = 3.0f * speed_mult;
        float accel = 0.5f * speed_mult;
        if (input.dx > 0) { gs->ice_vx += accel; if (gs->ice_vx > max_ice) gs->ice_vx = max_ice; }
        else if (input.dx < 0) { gs->ice_vx -= accel; if (gs->ice_vx < -max_ice) gs->ice_vx = -max_ice; }
        else {
            if (gs->ice_vx > accel) gs->ice_vx -= accel;
            else if (gs->ice_vx < -accel) gs->ice_vx += accel;
            else gs->ice_vx = 0.0f;
        }
        if (input.dy > 0) { gs->ice_vy += accel; if (gs->ice_vy > max_ice) gs->ice_vy = max_ice; }
        else if (input.dy < 0) { gs->ice_vy -= accel; if (gs->ice_vy < -max_ice) gs->ice_vy = -max_ice; }
        else {
            if (gs->ice_vy > accel) gs->ice_vy -= accel;
            else if (gs->ice_vy < -accel) gs->ice_vy += accel;
            else gs->ice_vy = 0.0f;
        }
        dx = gs->ice_vx; dy = gs->ice_vy;
    } else {
        gs->ice_vx = 0.0f; gs->ice_vy = 0.0f;
        float spd = (terr == TERR_WATER ? 1.0f : 2.0f) * speed_mult;
        if (input.dx != 0) dx = (input.dx > 0) ? spd : -spd;
        if (input.dy != 0) dy = (input.dy > 0) ? spd : -spd;
    }
    if (dx != 0.0f) { gs->player_x += dx; resolve_walls(gs, &gs->player_x, &gs->player_y, true, dx); }
    if (dy != 0.0f) { gs->player_y += dy; resolve_walls(gs, &gs->player_x, &gs->player_y, false, dy); }

    float pleft = gs->player_x - hs, ptop = gs->player_y - hs;
    if (ptop < 40.0f && gs->cur_row > 1) { gs->player_y = 539.0f + hs; gs->warp_col = gs->cur_col; gs->warp_row = gs->cur_row - 1; gs->warp_place_at_spawn = false; return evt | GAME_EVT_ROOM_WARP; }
    if (ptop > 539.0f && gs->cur_row < 5) { gs->player_y = 40.0f + hs; gs->warp_col = gs->cur_col; gs->warp_row = gs->cur_row + 1; gs->warp_place_at_spawn = false; return evt | GAME_EVT_ROOM_WARP; }
    if (pleft < 0.0f && gs->cur_col > 'A') { gs->player_x = 778.0f + hs; gs->warp_col = gs->cur_col - 1; gs->warp_row = gs->cur_row; gs->warp_place_at_spawn = false; return evt | GAME_EVT_ROOM_WARP; }
    if (pleft > 778.0f && gs->cur_col < 'E') { gs->player_x = 0.0f + hs; gs->warp_col = gs->cur_col + 1; gs->warp_row = gs->cur_row; gs->warp_place_at_spawn = false; return evt | GAME_EVT_ROOM_WARP; }
    if (gs->player_x < hs) gs->player_x = hs;
    if (gs->player_x > lvl->width - hs) gs->player_x = lvl->width - hs;
    if (gs->player_y < 40.0f + hs) gs->player_y = 40.0f + hs;
    if (gs->player_y > 560.0f - hs) gs->player_y = 560.0f - hs;

    uint8_t cur_terr = get_terrain_at(lvl, gs->player_x, gs->player_y);
    if (cur_terr == TERR_WATER && !gs->invincible) {
        if (!gs->in_water) { gs->in_water = true; gs->water_ticks = WATER_MAX_TICKS; }
        if (gs->water_ticks > 0) gs->water_ticks--;
        else { gs->dying = true; return evt | GAME_EVT_DIE_WATER; }
    } else {
        gs->in_water = false; gs->water_ticks = WATER_MAX_TICKS;
    }

    if (!gs->invincible && cur_terr == TERR_PIT &&
        find_mp_at(lvl, gs->enemy_frame, gs->player_x, gs->player_y) < 0) {
        float h = hs - 4.0f;
        bool safe = (get_terrain_at(lvl, gs->player_x - h, gs->player_y - h) != TERR_PIT) ||
                    (get_terrain_at(lvl, gs->player_x + h, gs->player_y - h) != TERR_PIT) ||
                    (get_terrain_at(lvl, gs->player_x - h, gs->player_y + h) != TERR_PIT) ||
                    (get_terrain_at(lvl, gs->player_x + h, gs->player_y + h) != TERR_PIT) ||
                    (find_mp_at(lvl, gs->enemy_frame, gs->player_x - h, gs->player_y - h) >= 0) ||
                    (find_mp_at(lvl, gs->enemy_frame, gs->player_x + h, gs->player_y + h) >= 0);
        if (!safe) { gs->dying = true; return evt | GAME_EVT_DIE_PIT; }
    }

    for (uint16_t i = 0; !gs->invincible && i < lvl->num_enemies; i++) {
        float ex, ey;
        level_get_enemy_pos(&lvl->enemies[i], gs->enemy_frame, &ex, &ey);
        if (phys_square_circle_overlap(gs->player_x, gs->player_y, hs - 2.0f, ex, ey, lvl->enemies[i].radius)) {
            gs->dying = true;
            return evt | GAME_EVT_DIE;
        }
    }

    int ridx = room_index(gs->cur_col, gs->cur_row);
    for (uint16_t i = 0; i < lvl->num_coins && i < MAX_ROOM_COINS; i++) {
        if (gs->room_coins_collected[ridx][i]) continue;
        if (phys_square_circle_overlap(gs->player_x, gs->player_y, hs, (float)lvl->coins[i].x, (float)lvl->coins[i].y, COIN_RADIUS)) {
            gs->room_coins_collected[ridx][i] = true;
            gs->coins_collected_count++;
            update_gold_door_whg4(gs);
            evt |= GAME_EVT_COIN;
        }
    }
    for (uint16_t i = 0; i < lvl->num_diamonds && i < MAX_ROOM_DIAMONDS; i++) {
        if (gs->room_diamonds_collected[ridx][i]) continue;
        if (phys_square_circle_overlap(gs->player_x, gs->player_y, hs, (float)lvl->diamonds[i].x, (float)lvl->diamonds[i].y, DIAMOND_RADIUS)) {
            gs->room_diamonds_collected[ridx][i] = true;
            gs->diamonds_collected_count++;
            update_gold_door_whg4(gs);
            evt |= GAME_EVT_DIAMOND;
        }
    }
    for (uint16_t i = 0; i < lvl->num_keys; i++) {
        uint16_t kid = lvl->keys[i].door_id;
        if (kid < MAX_DOORS && gs->whg3_key_collected[kid]) continue;
        if (phys_rect_overlap(gs->player_x - hs, gs->player_y - hs, hs * 2.0f, hs * 2.0f,
                              (float)lvl->keys[i].x - KEY_HALF_SIZE, (float)lvl->keys[i].y - KEY_HALF_SIZE,
                              KEY_HALF_SIZE * 2.0f, KEY_HALF_SIZE * 2.0f)) {
            if (kid < MAX_DOORS) { gs->whg3_key_collected[kid] = true; gs->door_open[kid] = true; }
            evt |= GAME_EVT_KEY;
        }
    }

    const Whg4StartFinish* fin = &WHG4_FINISHES[gs->cur_level - 1];
    bool inside_spawn_check = false;
    for (uint16_t i = 0; i < lvl->num_checks; i++) {
        const CheckRect* c = &lvl->checks[i];
        bool in_check = phys_rect_overlap(gs->player_x - hs, gs->player_y - hs, hs * 2.0f, hs * 2.0f,
                                          (float)c->x, (float)c->y, (float)c->w, (float)c->h);
        bool is_spawn_check = c->kind == gs->spawn_check_idx &&
                             gs->cur_col == gs->spawn_col && gs->cur_row == gs->spawn_row;
        if (is_spawn_check && in_check && !gs->left_spawn_check) {
            inside_spawn_check = true;
            continue;
        }
        if (!in_check) continue;
        bool is_finish = gs->cur_col == fin->col && gs->cur_row == fin->row && c->kind == fin->check_idx;
        if (is_finish) {
            if (game_total_cash(gs) >= WHG4_LEVEL_CASH[gs->cur_level - 1]) {
                gs->level_won = true;
                return evt | GAME_EVT_WIN;
            }
            continue;
        }
        bool newly_locked = false;
        for (uint16_t ci = 0; ci < lvl->num_coins && ci < MAX_ROOM_COINS; ci++) {
            if (gs->room_coins_collected[ridx][ci] && !gs->room_coins_locked[ridx][ci]) {
                gs->room_coins_locked[ridx][ci] = true;
                newly_locked = true;
            }
        }
        for (uint16_t di = 0; di < lvl->num_diamonds && di < MAX_ROOM_DIAMONDS; di++) {
            if (gs->room_diamonds_collected[ridx][di] && !gs->room_diamonds_locked[ridx][di]) {
                gs->room_diamonds_locked[ridx][di] = true;
                newly_locked = true;
            }
        }
        for (int k = 0; k < MAX_DOORS; k++) {
            if (gs->whg3_key_collected[k] && !gs->whg3_key_locked[k]) {
                gs->whg3_key_locked[k] = true;
                newly_locked = true;
            }
        }
        if (gs->spawn_col != gs->cur_col || gs->spawn_row != gs->cur_row || gs->spawn_check_idx != c->kind || newly_locked) {
            gs->spawn_col = gs->cur_col;
            gs->spawn_row = gs->cur_row;
            gs->spawn_check_idx = (uint8_t)c->kind;
            evt |= GAME_EVT_CHECKPOINT;
        }
    }
    if (!inside_spawn_check) gs->left_spawn_check = true;
    return evt;
}

uint32_t game_update(GameState* gs, GameInput input, float speed_mult) {
    if (gs->is_whg4) return game_update_whg4(gs, input, speed_mult);
    uint32_t events = GAME_EVT_NONE;
    if (gs->level_won) return events;
    if (gs->invincible && gs->dying) {
        gs->dying = false;
        gs->player_alpha = 100;
    }

    const LevelData* lvl = gs->lvl;
    float hs = gs->player_half_size;
    gs->level_ticks++;
    gs->enemy_frame += 0.5f;

    if (gs->dying) {
        if (gs->player_alpha > 2) {
            gs->player_alpha -= 2;
        } else {
            gs->deaths++;
            gs->level_deaths++;
            if (gs->is_whg3) {
                reset_unlocked_whg3(gs);
                gs->warp_col = gs->spawn_col;
                gs->warp_row = gs->spawn_row;
                gs->warp_check_idx = gs->spawn_check_idx;
                events |= GAME_EVT_ROOM_WARP;
            } else {
                respawn_at_checkpoint(gs);
            }
        }
        return events;
    }

    float spd = gs->base_speed * speed_mult;
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

    if (gs->player_x < hs) gs->player_x = hs;
    if (gs->player_x > lvl->width - hs) gs->player_x = lvl->width - hs;
    if (gs->player_y < hs) gs->player_y = hs;
    if (gs->player_y > lvl->height - hs) gs->player_y = lvl->height - hs;

    if (gs->is_whg3) {
        int ridx = room_index(gs->cur_col, gs->cur_row);
        uint16_t max_c = lvl->num_coins < MAX_ROOM_COINS ? lvl->num_coins : MAX_ROOM_COINS;
        for (uint16_t i = 0; i < max_c; i++) {
            if (gs->room_coins_collected[ridx][i]) continue;
            if (phys_square_circle_overlap(gs->player_x, gs->player_y, hs,
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
            if (kid < MAX_DOORS && gs->whg3_key_collected[kid]) continue;
            if (phys_rect_overlap(gs->player_x - hs, gs->player_y - hs, hs * 2.0f, hs * 2.0f,
                                  (float)lvl->keys[i].x - KEY_HALF_SIZE, (float)lvl->keys[i].y - KEY_HALF_SIZE,
                                  KEY_HALF_SIZE * 2.0f, KEY_HALF_SIZE * 2.0f)) {
                if (kid < MAX_DOORS) {
                    gs->whg3_key_collected[kid] = true;
                    gs->door_open[kid] = true;
                }
                events |= GAME_EVT_KEY;
            }
        }

        const Whg3StartFinish* fin = &WHG3_FINISHES[gs->cur_level - 1];
        bool inside_spawn_check = false;
        bool inside_room_entry_check = false;
        for (uint16_t i = 0; i < lvl->num_checks; i++) {
            const CheckRect* c = &lvl->checks[i];
            bool in_check = phys_rect_overlap(gs->player_x - hs, gs->player_y - hs, hs * 2.0f, hs * 2.0f,
                                              (float)c->x, (float)c->y, (float)c->w, (float)c->h);
            bool is_spawn_check = c->kind == gs->spawn_check_idx &&
                                 gs->cur_col == gs->spawn_col && gs->cur_row == gs->spawn_row;
            if (c->kind == gs->room_entry_check_idx && in_check && !gs->left_room_entry_check) {
                inside_room_entry_check = true;
                if (is_spawn_check) inside_spawn_check = true;
                continue;
            }
            if (is_spawn_check && in_check && !gs->left_spawn_check) {
                inside_spawn_check = true;
                continue;
            }
            if (!in_check) continue;
            bool is_finish = gs->cur_col == fin->col && gs->cur_row == fin->row && c->kind == fin->check_idx;
            if (is_finish) {
                if (gs->coins_collected_count >= WHG3_LEVEL_COINS[gs->cur_level - 1]) {
                    gs->level_won = true;
                    events |= GAME_EVT_WIN;
                    return events;
                }
                continue;
            }
            for (size_t e = 0; e < WHG3_NUM_ENTRANCES; e++) {
                const Whg3Entrance* ent = &WHG3_ENTRANCES[e];
                if (ent->level == gs->cur_level && ent->col == gs->cur_col &&
                    ent->row == gs->cur_row && ent->check_idx == c->kind) {
                    gs->warp_col = ent->dst_col;
                    gs->warp_row = ent->dst_row;
                    gs->warp_check_idx = ent->dst_check_idx;
                    events |= GAME_EVT_ROOM_WARP;
                    return events;
                }
            }
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
                    if (gs->whg3_key_collected[k]) gs->whg3_key_locked[k] = true;
                }
                events |= GAME_EVT_CHECKPOINT;
            }
        }
        if (!inside_room_entry_check) gs->left_room_entry_check = true;
        if (!inside_spawn_check) gs->left_spawn_check = true;
    } else {
        for (uint16_t i = 0; i < lvl->num_coins; i++) {
            if (gs->coin_collected[i]) continue;
            if (phys_square_circle_overlap(gs->player_x, gs->player_y, hs,
                                           (float)lvl->coins[i].x, (float)lvl->coins[i].y, COIN_RADIUS)) {
                gs->coin_collected[i] = true;
                gs->coins_collected_count++;
                events |= GAME_EVT_COIN;
            }
        }

        for (uint16_t i = 0; i < lvl->num_keys; i++) {
            if (gs->key_collected[i]) continue;
            if (phys_rect_overlap(gs->player_x - hs, gs->player_y - hs, hs * 2.0f, hs * 2.0f,
                                  (float)lvl->keys[i].x - KEY_HALF_SIZE, (float)lvl->keys[i].y - KEY_HALF_SIZE,
                                  KEY_HALF_SIZE * 2.0f, KEY_HALF_SIZE * 2.0f)) {
                gs->key_collected[i] = true;
                if (lvl->keys[i].door_id < MAX_DOORS) gs->door_open[lvl->keys[i].door_id] = true;
                events |= GAME_EVT_KEY;
            }
        }

        bool has_goal = false;
        for (uint16_t i = 0; i < lvl->num_checks; i++) {
            if (lvl->checks[i].kind == 2) { has_goal = true; break; }
        }
        for (uint16_t i = 0; i < lvl->num_checks; i++) {
            const CheckRect* c = &lvl->checks[i];
            bool in_chk = phys_rect_overlap(gs->player_x - hs, gs->player_y - hs, hs * 2.0f, hs * 2.0f,
                                            (float)c->x, (float)c->y, (float)c->w, (float)c->h);
            if (c->kind == 1 && !has_goal) {
                if (!in_chk) gs->left_start_check = true;
                else if (gs->left_start_check && gs->coins_collected_count >= lvl->num_coins) {
                    gs->level_won = true;
                    events |= GAME_EVT_WIN;
                    return events;
                }
            }
            if (!in_chk) continue;
            if (c->kind == 2) {
                if (gs->coins_collected_count >= lvl->num_coins) {
                    gs->level_won = true;
                    events |= GAME_EVT_WIN;
                    return events;
                }
            } else if (c->kind == 3) {
                float nx = c->x + c->w * 0.5f;
                float ny = c->y + c->h * 0.5f;
                if (gs->spawn_x != nx || gs->spawn_y != ny) {
                    gs->spawn_x = nx;
                    gs->spawn_y = ny;
                    for (uint16_t j = 0; j < lvl->num_coins; j++) {
                        if (gs->coin_collected[j]) gs->coin_locked[j] = true;
                    }
                    events |= GAME_EVT_CHECKPOINT;
                }
            }
        }
    }

    for (uint16_t i = 0; !gs->invincible && i < lvl->num_enemies; i++) {
        float ex = 0, ey = 0;
        level_get_enemy_pos(&lvl->enemies[i], gs->enemy_frame, &ex, &ey);
        if (phys_square_circle_overlap(gs->player_x, gs->player_y, hs,
                                       ex, ey, lvl->enemies[i].radius)) {
            gs->dying = true;
            events |= GAME_EVT_DIE;
            break;
        }
    }

    return events;
}
