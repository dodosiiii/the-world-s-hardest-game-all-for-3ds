#include "level.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool read_u16(FILE* f, uint16_t* out) {
    uint8_t b[2];
    if (fread(b, 1, 2, f) != 2) return false;
    *out = (uint16_t)(b[0] | (b[1] << 8));
    return true;
}

static bool read_i16(FILE* f, int16_t* out) {
    uint16_t u;
    if (!read_u16(f, &u)) return false;
    *out = (int16_t)u;
    return true;
}

bool level_load(LevelData* lvl, const char* path) {
    memset(lvl, 0, sizeof(*lvl));
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    uint8_t magic[4];
    uint16_t ver = 0, flags = 0, res = 0;
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "WHG2", 4) != 0) goto fail;
    if (!read_u16(f, &ver) || ver != 1) goto fail;
    if (!read_u16(f, &lvl->id)) goto fail;
    if (!read_u16(f, &lvl->width)) goto fail;
    if (!read_u16(f, &lvl->height)) goto fail;
    if (!read_u16(f, &flags) || !read_u16(f, &res)) goto fail;

    if (!read_u16(f, &lvl->num_walls)) goto fail;
    if (lvl->num_walls > 0) {
        lvl->walls = (Rect16*)malloc(sizeof(Rect16) * lvl->num_walls);
        if (!lvl->walls) goto fail;
        for (uint16_t i = 0; i < lvl->num_walls; i++) {
            if (!read_i16(f, &lvl->walls[i].x) || !read_i16(f, &lvl->walls[i].y) ||
                !read_i16(f, &lvl->walls[i].w) || !read_i16(f, &lvl->walls[i].h)) goto fail;
        }
    }

    if (!read_u16(f, &lvl->num_doors)) goto fail;
    if (lvl->num_doors > 0) {
        lvl->doors = (DoorRect*)malloc(sizeof(DoorRect) * lvl->num_doors);
        if (!lvl->doors) goto fail;
        for (uint16_t i = 0; i < lvl->num_doors; i++) {
            if (!read_u16(f, &lvl->doors[i].door_id) ||
                !read_i16(f, &lvl->doors[i].x) || !read_i16(f, &lvl->doors[i].y) ||
                !read_i16(f, &lvl->doors[i].w) || !read_i16(f, &lvl->doors[i].h)) goto fail;
        }
    }

    if (!read_u16(f, &lvl->num_checks)) goto fail;
    if (lvl->num_checks > 0) {
        lvl->checks = (CheckRect*)malloc(sizeof(CheckRect) * lvl->num_checks);
        if (!lvl->checks) goto fail;
        for (uint16_t i = 0; i < lvl->num_checks; i++) {
            if (!read_u16(f, &lvl->checks[i].kind) ||
                !read_i16(f, &lvl->checks[i].x) || !read_i16(f, &lvl->checks[i].y) ||
                !read_i16(f, &lvl->checks[i].w) || !read_i16(f, &lvl->checks[i].h)) goto fail;
        }
    }

    if (!read_u16(f, &lvl->num_coins)) goto fail;
    if (lvl->num_coins > 0) {
        lvl->coins = (Point16*)malloc(sizeof(Point16) * lvl->num_coins);
        if (!lvl->coins) goto fail;
        for (uint16_t i = 0; i < lvl->num_coins; i++) {
            if (!read_i16(f, &lvl->coins[i].x) || !read_i16(f, &lvl->coins[i].y)) goto fail;
        }
    }

    if (!read_u16(f, &lvl->num_keys)) goto fail;
    if (lvl->num_keys > 0) {
        lvl->keys = (KeyDef*)malloc(sizeof(KeyDef) * lvl->num_keys);
        if (!lvl->keys) goto fail;
        for (uint16_t i = 0; i < lvl->num_keys; i++) {
            if (!read_u16(f, &lvl->keys[i].door_id) ||
                !read_i16(f, &lvl->keys[i].x) || !read_i16(f, &lvl->keys[i].y)) goto fail;
        }
    }

    if (!read_u16(f, &lvl->num_enemies)) goto fail;
    if (lvl->num_enemies > 0) {
        lvl->enemies = (EnemyTrack*)calloc(lvl->num_enemies, sizeof(EnemyTrack));
        if (!lvl->enemies) goto fail;
        for (uint16_t i = 0; i < lvl->num_enemies; i++) {
            uint16_t nf = 0, rad10 = 0;
            if (!read_u16(f, &nf) || !read_u16(f, &rad10)) goto fail;
            lvl->enemies[i].num_frames = nf;
            lvl->enemies[i].radius = rad10 * 0.1f;
            if (nf > 0) {
                lvl->enemies[i].x2 = (int16_t*)malloc(sizeof(int16_t) * nf);
                lvl->enemies[i].y2 = (int16_t*)malloc(sizeof(int16_t) * nf);
                if (!lvl->enemies[i].x2 || !lvl->enemies[i].y2) goto fail;
                for (uint16_t t = 0; t < nf; t++) {
                    if (!read_i16(f, &lvl->enemies[i].x2[t]) ||
                        !read_i16(f, &lvl->enemies[i].y2[t])) goto fail;
                }
            }
        }
    }

    fclose(f);
    return true;

fail:
    fclose(f);
    level_free(lvl);
    return false;
}

void level_free(LevelData* lvl) {
    if (!lvl) return;
    free(lvl->walls);
    free(lvl->doors);
    free(lvl->checks);
    free(lvl->coins);
    free(lvl->keys);
    if (lvl->enemies) {
        for (uint16_t i = 0; i < lvl->num_enemies; i++) {
            free(lvl->enemies[i].x2);
            free(lvl->enemies[i].y2);
        }
        free(lvl->enemies);
    }
    memset(lvl, 0, sizeof(*lvl));
}

void level_get_enemy_pos(const EnemyTrack* e, float anim_frame, float* out_x, float* out_y) {
    if (e->num_frames == 0) {
        *out_x = 0.0f;
        *out_y = 0.0f;
        return;
    }
    if (e->num_frames == 1) {
        *out_x = e->x2[0] * 0.5f;
        *out_y = e->y2[0] * 0.5f;
        return;
    }
    uint32_t f0 = ((uint32_t)anim_frame) % e->num_frames;
    uint32_t f1 = (f0 + 1) % e->num_frames;
    float frac = anim_frame - (float)((uint32_t)anim_frame);
    float x0 = e->x2[f0] * 0.5f;
    float y0 = e->y2[f0] * 0.5f;
    float x1 = e->x2[f1] * 0.5f;
    float y1 = e->y2[f1] * 0.5f;
    *out_x = x0 + (x1 - x0) * frac;
    *out_y = y0 + (y1 - y0) * frac;
}
