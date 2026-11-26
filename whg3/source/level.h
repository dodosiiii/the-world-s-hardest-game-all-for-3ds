#ifndef LEVEL_H
#define LEVEL_H

#include <stdint.h>
#include <stdbool.h>

#define LEVEL_WIDTH  900
#define LEVEL_HEIGHT 600
#define TOTAL_MAIN_LEVELS 5
#define TOTAL_LEVELS 5

typedef struct {
    int16_t x, y, w, h;
} Rect16;

typedef struct {
    uint16_t door_id;
    int16_t x, y, w, h;
} DoorRect;

typedef struct {
    uint16_t kind; // check index c0, c1, c2...
    int16_t x, y, w, h;
} CheckRect;

typedef struct {
    int16_t x, y;
} Point16;

typedef struct {
    uint16_t door_id;
    int16_t x, y;
} KeyDef;

typedef struct {
    uint16_t num_frames;
    float radius;
    int16_t* x2;
    int16_t* y2;
} EnemyTrack;

typedef struct {
    uint16_t id;
    uint16_t width;
    uint16_t height;

    uint16_t num_walls;
    Rect16* walls;

    uint16_t num_doors;
    DoorRect* doors;

    uint16_t num_checks;
    CheckRect* checks;

    uint16_t num_coins;
    Point16* coins;

    uint16_t num_keys;
    KeyDef* keys;

    uint16_t num_enemies;
    EnemyTrack* enemies;
} LevelData;

bool level_load(LevelData* lvl, const char* path);
void level_free(LevelData* lvl);
void level_get_enemy_pos(const EnemyTrack* e, float anim_frame, float* out_x, float* out_y);

#endif // LEVEL_H
