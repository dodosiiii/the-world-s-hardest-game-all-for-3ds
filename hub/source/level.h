#ifndef LEVEL_H
#define LEVEL_H

#include <stdint.h>
#include <stdbool.h>

#define LEVEL_WIDTH  550
#define LEVEL_HEIGHT 400
#define TOTAL_MAIN_LEVELS 30
#define TOTAL_LEVELS 30

enum {
    TERR_NONE  = 0,
    TERR_WATER = 1,
    TERR_ICE   = 2,
    TERR_PIT   = 3,
    TERR_CBR1  = 4,
    TERR_CBR2  = 5,
    TERR_CBR3  = 6,
    TERR_CBL1  = 7,
    TERR_CBL2  = 8,
    TERR_CBL3  = 9,
    TERR_CBU1  = 10,
    TERR_CBU2  = 11,
    TERR_CBU3  = 12,
    TERR_CBD1  = 13,
    TERR_CBD2  = 14,
    TERR_CBD3  = 15
};

typedef struct {
    int16_t x, y, w, h;
} Rect16;

typedef struct {
    uint16_t door_id;
    int16_t x, y, w, h;
} DoorRect;

typedef struct {
    uint16_t kind; // WHG1/2: 1=start, 2=goal, 3+=checkpoint; WHG3/4: room check index
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
    uint8_t type;
    int16_t x, y, w, h;
} TerrainRect;

typedef struct {
    uint16_t num_frames;
    int16_t w, h;
    int16_t* x;
    int16_t* y;
} MovingPlatform;

typedef struct {
    uint16_t num_frames;
    float radius;
    int16_t* x2; // coordinate * 2 (0.5 px precision)
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

    uint16_t num_diamonds;
    Point16* diamonds;

    uint16_t num_keys;
    KeyDef* keys;

    uint16_t num_terrains;
    TerrainRect* terrains;

    uint16_t num_mps;
    MovingPlatform* mps;

    uint16_t num_enemies;
    EnemyTrack* enemies;
} LevelData;

bool level_load(LevelData* lvl, const char* path);
void level_free(LevelData* lvl);
void level_get_enemy_pos(const EnemyTrack* e, float anim_frame, float* out_x, float* out_y);
void level_get_mp_pos(const MovingPlatform* mp, float anim_frame, float* out_x, float* out_y);

#endif // LEVEL_H
