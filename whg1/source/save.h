#ifndef SAVE_H
#define SAVE_H

#include <stdint.h>
#include <stdbool.h>

#define SAVE_PATH "sdmc:/3ds/whg1/save.bin"
#define SAVE_MAGIC 0x31474857u // "WHG1"
#define SAVE_VERSION 1

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t unlocked_level; // 1..30
    uint32_t total_deaths;
    uint8_t  muted;
    uint8_t  player_skin;    // 0..5
    uint8_t  show_timer;     // 0 or 1
    uint8_t  show_hud;       // 0 or 1
    int16_t  best_deaths[30]; // -1 = not beaten yet
} SaveData;

void save_defaults(SaveData* sd);
bool save_load(SaveData* sd);
bool save_write(const SaveData* sd);

#endif // SAVE_H
