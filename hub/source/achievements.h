#ifndef ACHIEVEMENTS_H
#define ACHIEVEMENTS_H

#include "save.h"
#include <stdint.h>
#include <stdbool.h>

#define ACHIEVEMENTS_PER_GAME 14

typedef enum {
    ACH_METRIC_WINS,
    ACH_METRIC_PERFECT_WINS,
    ACH_METRIC_COINS,
    ACH_METRIC_KEYS,
    ACH_METRIC_DIAMONDS,
    ACH_METRIC_CHECKPOINTS,
    ACH_METRIC_FAST_WINS
} AchievementMetric;

typedef struct {
    const char* name;
    const char* description;
    AchievementMetric metric;
    uint32_t target;
} AchievementDef;

const AchievementDef* achievement_get(int game, int index);
uint32_t achievement_progress(const SaveData* save, int game, int index);
uint8_t achievement_percent(const SaveData* save, int game, int index);
bool achievement_is_unlocked(const SaveData* save, int game, int index);
uint8_t achievement_completion_percent(const SaveData* save, int game);
uint32_t achievement_refresh(SaveData* save, int game);

#endif
