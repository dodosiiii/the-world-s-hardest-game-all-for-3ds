#ifndef GAME_REGISTRY_H
#define GAME_REGISTRY_H

#include <stdint.h>
#include <stdbool.h>

#define NUM_GAMES 4

typedef enum {
    GAME_WHG1 = 0,
    GAME_WHG2 = 1,
    GAME_WHG3 = 2,
    GAME_WHG4 = 3
} GameId;

typedef struct {
    const char* short_name;
    const char* title;
    const char* subtitle;
    const char* romfs_prefix;
    uint16_t main_levels;
    uint16_t tutorial_levels;
    uint16_t total_levels;
    bool available;
} GameDef;

static const GameDef g_games[NUM_GAMES] = {
    { "WHG 1", "HARDEST GAME 1", "30 Niveaux classiques",    "romfs:/whg1", 30, 0, 30, true  },
    { "WHG 2", "HARDEST GAME 2", "50 Niveaux + 4 Tutoriels", "romfs:/whg2", 50, 4, 54, true  },
    { "WHG 3", "HARDEST GAME 3", "5 Mondes (48 Salles)",     "romfs:/whg3", 5,  0, 5,  true  },
    { "WHG 4", "HARDEST GAME 4", "5 Mondes (60 Salles)",     "romfs:/whg4", 5,  0, 5,  true  },
};

#endif // GAME_REGISTRY_H
