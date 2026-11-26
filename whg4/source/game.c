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
            return;
        }
    }
    gs->player_x = lvl->width * 0.5f;
    gs->player_y = lvl->height * 0.5f;
}

uint16_t game_total_cash(const GameState* gs) {
    return gs->coins_collected_count + gs->diamonds_collected_count * 100;
}

static void update_gold_door(GameState* gs) {
    uint16_t cash = game_total_cash(gs);
    if (gs->cur_level == 5 && gs->cur_col == 'C' && gs->cur_row == 2) {
        gs->gold_door_open = (cash >= WHG4_L5_BOSS_CASH);
    } else {
        gs->gold_door_open = (cash >= WHG4_LEVEL_CASH[gs->cur_level - 1]);
    }
}

void game_enter_room(GameState* gs, const LevelData* room, char col, uint8_t row, bool place_at_spawn, uint8_t check_idx) {
    gs->lvl = room;
    gs->cur_col = col;
    gs->cur_row = row;
    int ridx = room_index(col, row);
    gs->room_discovered[ridx] = true;
    gs->coin_collected = gs->room_coins_collected[ridx];
    gs->diamond_collected = gs->room_diamonds_collected[ridx];
    gs->enemy_frame = 0.0f;
    gs->player_alpha = 100;
    gs->dying = false;
    gs->ice_vx = 0.0f;
    gs->ice_vy = 0.0f;
    update_gold_door(gs);
    if (place_at_spawn) {
        place_player_at_check(gs, check_idx);
    }
}

void game_init_level(GameState* gs, uint8_t level_num, const LevelData* first_room, uint32_t initial_deaths) {
    memset(gs, 0, sizeof(*gs));
    gs->cur_level = level_num;
    gs->deaths = initial_deaths;
    gs->water_ticks = WATER_MAX_TICKS;
    const Whg4StartFinish* st = &WHG4_STARTS[level_num - 1];
    gs->spawn_col = st->col;
    gs->spawn_row = st->row;
    gs->spawn_check_idx = st->check_idx;
    game_enter_room(gs, first_room, st->col, st->row, true, st->check_idx);
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
        for (int d = 0; d < MAX_ROOM_DIAMONDS; d++) {
            if (gs->room_diamonds_collected[r][d] && !gs->room_diamonds_locked[r][d]) {
                gs->room_diamonds_collected[r][d] = false;
                if (gs->diamonds_collected_count > 0) gs->diamonds_collected_count--;
            }
        }
    }
    for (int k = 0; k < MAX_DOORS; k++) {
        if (gs->key_collected[k] && !gs->key_locked[k]) {
            gs->key_collected[k] = false;
            gs->door_open[k] = false;
        }
    }
    update_gold_door(gs);
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

uint32_t game_update(GameState* gs, GameInput input, float speed_mult) {
    uint32_t evt = GAME_EVT_NONE;
    const LevelData* lvl = gs->lvl;
    if (!lvl) return evt;

    gs->level_ticks++;
    float prev_frame = gs->enemy_frame;
    gs->enemy_frame += 0.5f; // 30fps Flash timeline at 60fps

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
            reset_unlocked_on_death(gs);
            if (gs->cur_col != gs->spawn_col || gs->cur_row != gs->spawn_row) {
                gs->warp_col = gs->spawn_col;
                gs->warp_row = gs->spawn_row;
                gs->warp_check_idx = gs->spawn_check_idx;
                gs->warp_place_at_spawn = true;
                return evt | GAME_EVT_ROOM_WARP;
            }
            place_player_at_check(gs, gs->spawn_check_idx);
        }
        return evt;
    }

    // Moving platforms carry player
    int mp_idx = find_mp_at(lvl, prev_frame, gs->player_x, gs->player_y);
    if (mp_idx >= 0) {
        float x0, y0, x1, y1;
        level_get_mp_pos(&lvl->mps[mp_idx], prev_frame, &x0, &y0);
        level_get_mp_pos(&lvl->mps[mp_idx], gs->enemy_frame, &x1, &y1);
        float mdx = x1 - x0;
        float mdy = y1 - y0;
        if (mdx != 0.0f) {
            gs->player_x += mdx;
            resolve_walls(gs, &gs->player_x, &gs->player_y, true, mdx);
        }
        if (mdy != 0.0f) {
            gs->player_y += mdy;
            resolve_walls(gs, &gs->player_x, &gs->player_y, false, mdy);
        }
    }

    // Current terrain at center
    uint8_t terr = get_terrain_at(lvl, gs->player_x, gs->player_y);

    // Conveyor belt drift
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

    if (cbx != 0.0f) {
        gs->player_x += cbx;
        resolve_walls(gs, &gs->player_x, &gs->player_y, true, cbx);
    }
    if (cby != 0.0f) {
        gs->player_y += cby;
        resolve_walls(gs, &gs->player_x, &gs->player_y, false, cby);
    }

    // Player movement
    float dx = 0.0f, dy = 0.0f;
    if (terr == TERR_ICE) {
        float max_ice = ICE_SPEED_60FPS * speed_mult;
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
        dx = gs->ice_vx;
        dy = gs->ice_vy;
    } else {
        gs->ice_vx = 0.0f;
        gs->ice_vy = 0.0f;
        float spd = (terr == TERR_WATER ? WATER_SPEED_60FPS : PLAYER_SPEED_60FPS) * speed_mult;
        if (input.dx != 0) dx = (input.dx > 0) ? spd : -spd;
        if (input.dy != 0) dy = (input.dy > 0) ? spd : -spd;
    }

    if (dx != 0.0f) {
        gs->player_x += dx;
        resolve_walls(gs, &gs->player_x, &gs->player_y, true, dx);
    }
    if (dy != 0.0f) {
        gs->player_y += dy;
        resolve_walls(gs, &gs->player_x, &gs->player_y, false, dy);
    }

    // Screen-edge room transitions
    float pleft = gs->player_x - PLAYER_HALF_SIZE;
    float ptop  = gs->player_y - PLAYER_HALF_SIZE;
    if (ptop < 40.0f && gs->cur_row > 1) {
        gs->player_y = 539.0f + PLAYER_HALF_SIZE;
        gs->warp_col = gs->cur_col;
        gs->warp_row = gs->cur_row - 1;
        gs->warp_place_at_spawn = false;
        return evt | GAME_EVT_ROOM_WARP;
    }
    if (ptop > 539.0f && gs->cur_row < 5) {
        gs->player_y = 40.0f + PLAYER_HALF_SIZE;
        gs->warp_col = gs->cur_col;
        gs->warp_row = gs->cur_row + 1;
        gs->warp_place_at_spawn = false;
        return evt | GAME_EVT_ROOM_WARP;
    }
    if (pleft < 0.0f && gs->cur_col > 'A') {
        gs->player_x = 778.0f + PLAYER_HALF_SIZE;
        gs->warp_col = gs->cur_col - 1;
        gs->warp_row = gs->cur_row;
        gs->warp_place_at_spawn = false;
        return evt | GAME_EVT_ROOM_WARP;
    }
    if (pleft > 778.0f && gs->cur_col < 'E') {
        gs->player_x = 0.0f + PLAYER_HALF_SIZE;
        gs->warp_col = gs->cur_col + 1;
        gs->warp_row = gs->cur_row;
        gs->warp_place_at_spawn = false;
        return evt | GAME_EVT_ROOM_WARP;
    }
    if (gs->player_x < PLAYER_HALF_SIZE) gs->player_x = PLAYER_HALF_SIZE;
    if (gs->player_x > lvl->width - PLAYER_HALF_SIZE) gs->player_x = lvl->width - PLAYER_HALF_SIZE;
    if (gs->player_y < 40.0f + PLAYER_HALF_SIZE) gs->player_y = 40.0f + PLAYER_HALF_SIZE;
    if (gs->player_y > 560.0f - PLAYER_HALF_SIZE) gs->player_y = 560.0f - PLAYER_HALF_SIZE;

    // Water timer check
    uint8_t cur_terr = get_terrain_at(lvl, gs->player_x, gs->player_y);
    if (cur_terr == TERR_WATER) {
        if (!gs->in_water) {
            gs->in_water = true;
            gs->water_ticks = WATER_MAX_TICKS;
        }
        if (gs->water_ticks > 0) {
            gs->water_ticks--;
        } else {
            gs->dying = true;
            gs->deaths++;
            gs->level_deaths++;
            return evt | GAME_EVT_DIE_WATER;
        }
    } else {
        gs->in_water = false;
        gs->water_ticks = WATER_MAX_TICKS;
    }

    // Pit death check (safe if on moving platform)
    if (cur_terr == TERR_PIT && find_mp_at(lvl, gs->enemy_frame, gs->player_x, gs->player_y) < 0) {
        // Check corners too so walking near edge doesn't unfairly kill
        float h = PLAYER_HALF_SIZE - 4.0f;
        bool safe = (get_terrain_at(lvl, gs->player_x - h, gs->player_y - h) != TERR_PIT) ||
                    (get_terrain_at(lvl, gs->player_x + h, gs->player_y - h) != TERR_PIT) ||
                    (get_terrain_at(lvl, gs->player_x - h, gs->player_y + h) != TERR_PIT) ||
                    (get_terrain_at(lvl, gs->player_x + h, gs->player_y + h) != TERR_PIT) ||
                    (find_mp_at(lvl, gs->enemy_frame, gs->player_x - h, gs->player_y - h) >= 0) ||
                    (find_mp_at(lvl, gs->enemy_frame, gs->player_x + h, gs->player_y + h) >= 0);
        if (!safe) {
            gs->dying = true;
            gs->deaths++;
            gs->level_deaths++;
            return evt | GAME_EVT_DIE_PIT;
        }
    }

    // Enemy collision
    for (uint16_t i = 0; i < lvl->num_enemies; i++) {
        float ex, ey;
        level_get_enemy_pos(&lvl->enemies[i], gs->enemy_frame, &ex, &ey);
        if (phys_square_circle_overlap(gs->player_x, gs->player_y, PLAYER_HALF_SIZE - 2.0f,
                                       ex, ey, lvl->enemies[i].radius)) {
            gs->dying = true;
            gs->deaths++;
            gs->level_deaths++;
            return evt | GAME_EVT_DIE;
        }
    }

    // Coin collection
    for (uint16_t i = 0; i < lvl->num_coins && i < MAX_ROOM_COINS; i++) {
        if (gs->coin_collected[i]) continue;
        if (phys_square_circle_overlap(gs->player_x, gs->player_y, PLAYER_HALF_SIZE,
                                       (float)lvl->coins[i].x, (float)lvl->coins[i].y, COIN_RADIUS)) {
            gs->coin_collected[i] = true;
            gs->coins_collected_count++;
            update_gold_door(gs);
            evt |= GAME_EVT_COIN;
        }
    }

    // Diamond collection ($100 each)
    for (uint16_t i = 0; i < lvl->num_diamonds && i < MAX_ROOM_DIAMONDS; i++) {
        if (gs->diamond_collected[i]) continue;
        if (phys_square_circle_overlap(gs->player_x, gs->player_y, PLAYER_HALF_SIZE,
                                       (float)lvl->diamonds[i].x, (float)lvl->diamonds[i].y, DIAMOND_RADIUS)) {
            gs->diamond_collected[i] = true;
            gs->diamonds_collected_count++;
            update_gold_door(gs);
            evt |= GAME_EVT_DIAMOND;
        }
    }

    // Key collection
    for (uint16_t i = 0; i < lvl->num_keys; i++) {
        uint16_t did = lvl->keys[i].door_id;
        if (did < MAX_DOORS && gs->key_collected[did]) continue;
        if (phys_rect_overlap(gs->player_x - PLAYER_HALF_SIZE, gs->player_y - PLAYER_HALF_SIZE,
                              PLAYER_HALF_SIZE * 2.0f, PLAYER_HALF_SIZE * 2.0f,
                              (float)lvl->keys[i].x - KEY_HALF_SIZE,
                              (float)lvl->keys[i].y - KEY_HALF_SIZE,
                              KEY_HALF_SIZE * 2.0f, KEY_HALF_SIZE * 2.0f)) {
            if (did < MAX_DOORS) {
                gs->key_collected[did] = true;
                gs->door_open[did] = true;
            }
            evt |= GAME_EVT_KEY;
        }
    }

    // Checkpoints and Goal
    const Whg4StartFinish* fin = &WHG4_FINISHES[gs->cur_level - 1];
    for (uint16_t i = 0; i < lvl->num_checks; i++) {
        const CheckRect* c = &lvl->checks[i];
        if (!phys_rect_overlap(gs->player_x - PLAYER_HALF_SIZE, gs->player_y - PLAYER_HALF_SIZE,
                               PLAYER_HALF_SIZE * 2.0f, PLAYER_HALF_SIZE * 2.0f,
                               (float)c->x, (float)c->y, (float)c->w, (float)c->h)) {
            continue;
        }
        if (gs->cur_col == fin->col && gs->cur_row == fin->row && c->kind == fin->check_idx) {
            gs->level_won = true;
            return evt | GAME_EVT_WIN;
        }
        int cur_ridx = room_index(gs->cur_col, gs->cur_row);
        bool newly_locked = false;
        for (uint16_t ci = 0; ci < lvl->num_coins && ci < MAX_ROOM_COINS; ci++) {
            if (gs->room_coins_collected[cur_ridx][ci] && !gs->room_coins_locked[cur_ridx][ci]) {
                gs->room_coins_locked[cur_ridx][ci] = true;
                newly_locked = true;
            }
        }
        for (uint16_t di = 0; di < lvl->num_diamonds && di < MAX_ROOM_DIAMONDS; di++) {
            if (gs->room_diamonds_collected[cur_ridx][di] && !gs->room_diamonds_locked[cur_ridx][di]) {
                gs->room_diamonds_locked[cur_ridx][di] = true;
                newly_locked = true;
            }
        }
        for (int k = 0; k < MAX_DOORS; k++) {
            if (gs->key_collected[k] && !gs->key_locked[k]) {
                gs->key_locked[k] = true;
                newly_locked = true;
            }
        }
        if (gs->spawn_col != gs->cur_col || gs->spawn_row != gs->cur_row ||
            gs->spawn_check_idx != c->kind || newly_locked) {
            gs->spawn_col = gs->cur_col;
            gs->spawn_row = gs->cur_row;
            gs->spawn_check_idx = (uint8_t)c->kind;
            evt |= GAME_EVT_CHECKPOINT;
        }
    }

    return evt;
}
