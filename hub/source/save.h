#ifndef SAVE_H
#define SAVE_H

#include "game_registry.h"
#include <stdint.h>
#include <stdbool.h>

#define SAVE_PATH "sdmc:/3ds/whg_hub/save.bin"
#define SAVE_TMP_PATH "sdmc:/3ds/whg_hub/save.tmp"
#define SAVE_MAGIC 0x48474857u // "WHGH"
#define SAVE_VERSION 5
#define MAX_LEVELS_PER_GAME 64
#define LANGUAGE_UNSET 0xFF

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t  muted;
    uint8_t  player_skin;    // 0..5
    uint8_t  show_timer;     // 0 or 1
    uint8_t  show_hud;       // 0 or 1
    uint8_t  speed_mode;     // 0 = 1.0x (Normal), 1 = 1.25x (Rapide), 2 = 1.5x (Turbo)
    uint8_t  zoom_mode;      // 0 = 1.0x (Global), 1 = 1.5x (Suivi), 2 = 2.0x (Proche)
    uint16_t unlocked_level[NUM_GAMES];
    uint32_t total_deaths[NUM_GAMES];
    int16_t  best_deaths[NUM_GAMES][MAX_LEVELS_PER_GAME];
    uint32_t achievement_unlocked[NUM_GAMES];
    uint32_t achievement_coins[NUM_GAMES];
    uint32_t achievement_keys[NUM_GAMES];
    uint32_t achievement_diamonds[NUM_GAMES];
    uint32_t achievement_checkpoints[NUM_GAMES];
    uint8_t language;
    uint8_t discovery_mode;
    uint8_t fx_enabled;      // 0 or 1 : particules + tremblement d'ecran
    uint32_t best_ticks[NUM_GAMES][MAX_LEVELS_PER_GAME]; // meilleur temps par niveau (images a 60 Hz), 0 = aucun
} SaveData;

void save_defaults(SaveData* sd);
bool save_load(SaveData* sd);
bool save_write(const SaveData* sd);

#endif // SAVE_H
