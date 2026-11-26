#include "achievements.h"
#include <stddef.h>

#define A(name, description, metric, target) { name, description, metric, target }

static const AchievementDef s_achievements[NUM_GAMES][ACHIEVEMENTS_PER_GAME] = {
    {
        A("Premier pas", "Termine un niveau.", ACH_METRIC_WINS, 1),
        A("En route", "Termine 5 niveaux.", ACH_METRIC_WINS, 5),
        A("A mi-chemin", "Termine 15 niveaux.", ACH_METRIC_WINS, 15),
        A("Jeu termine", "Termine les 30 niveaux.", ACH_METRIC_WINS, 30),
        A("Sans faute", "Termine un niveau sans mourir.", ACH_METRIC_PERFECT_WINS, 1),
        A("As du carre", "Termine 10 niveaux sans mourir.", ACH_METRIC_PERFECT_WINS, 10),
        A("Ramasseur", "Ramasse 25 pieces.", ACH_METRIC_COINS, 25),
        A("Collectionneur", "Ramasse 100 pieces.", ACH_METRIC_COINS, 100),
        A("Tresor", "Ramasse 500 pieces.", ACH_METRIC_COINS, 500),
        A("Premiere cle", "Ramasse une cle.", ACH_METRIC_KEYS, 1),
        A("Porte-cle", "Ramasse 10 cles.", ACH_METRIC_KEYS, 10),
        A("Point de repere", "Atteins 10 checkpoints.", ACH_METRIC_CHECKPOINTS, 10),
        A("Petit chrono", "Termine 3 niveaux en moins d'une minute.", ACH_METRIC_FAST_WINS, 3),
        A("Fonceur", "Termine 10 niveaux en moins d'une minute.", ACH_METRIC_FAST_WINS, 10),
    },
    {
        A("Premier pas", "Termine un niveau.", ACH_METRIC_WINS, 1),
        A("En route", "Termine 10 niveaux.", ACH_METRIC_WINS, 10),
        A("Marathon", "Termine 25 niveaux.", ACH_METRIC_WINS, 25),
        A("Jeu termine", "Termine les 50 niveaux.", ACH_METRIC_WINS, 50),
        A("Sans faute", "Termine un niveau sans mourir.", ACH_METRIC_PERFECT_WINS, 1),
        A("As du carre", "Termine 10 niveaux sans mourir.", ACH_METRIC_PERFECT_WINS, 10),
        A("Ramasseur", "Ramasse 50 pieces.", ACH_METRIC_COINS, 50),
        A("Collectionneur", "Ramasse 200 pieces.", ACH_METRIC_COINS, 200),
        A("Tresor", "Ramasse 1000 pieces.", ACH_METRIC_COINS, 1000),
        A("Premiere cle", "Ramasse une cle.", ACH_METRIC_KEYS, 1),
        A("Porte-cle", "Ramasse 15 cles.", ACH_METRIC_KEYS, 15),
        A("Point de repere", "Atteins 25 checkpoints.", ACH_METRIC_CHECKPOINTS, 25),
        A("Petit chrono", "Termine 3 niveaux en moins d'une minute.", ACH_METRIC_FAST_WINS, 3),
        A("Fonceur", "Termine 10 niveaux en moins d'une minute.", ACH_METRIC_FAST_WINS, 10),
    },
    {
        A("Premier monde", "Termine un monde.", ACH_METRIC_WINS, 1),
        A("Explorateur", "Termine 3 mondes.", ACH_METRIC_WINS, 3),
        A("Les 5 mondes", "Termine les 5 mondes.", ACH_METRIC_WINS, 5),
        A("Sans faute", "Termine un monde sans mourir.", ACH_METRIC_PERFECT_WINS, 1),
        A("As du carre", "Termine 3 mondes sans mourir.", ACH_METRIC_PERFECT_WINS, 3),
        A("Ramasseur", "Ramasse 50 pieces.", ACH_METRIC_COINS, 50),
        A("Tresor", "Ramasse 250 pieces.", ACH_METRIC_COINS, 250),
        A("Premiere cle", "Ramasse une cle.", ACH_METRIC_KEYS, 1),
        A("Porte-cle", "Ramasse 10 cles.", ACH_METRIC_KEYS, 10),
        A("Point de repere", "Atteins 10 checkpoints.", ACH_METRIC_CHECKPOINTS, 10),
        A("Boussole", "Atteins 25 checkpoints.", ACH_METRIC_CHECKPOINTS, 25),
        A("Randonneur", "Atteins 50 checkpoints.", ACH_METRIC_CHECKPOINTS, 50),
        A("Petit chrono", "Termine 1 monde en moins de 10 minutes.", ACH_METRIC_FAST_WINS, 1),
        A("Fonceur", "Termine 3 mondes en moins de 10 minutes.", ACH_METRIC_FAST_WINS, 3),
    },
    {
        A("Premier monde", "Termine un monde.", ACH_METRIC_WINS, 1),
        A("Explorateur", "Termine 3 mondes.", ACH_METRIC_WINS, 3),
        A("Les 5 mondes", "Termine les 5 mondes.", ACH_METRIC_WINS, 5),
        A("Sans faute", "Termine un monde sans mourir.", ACH_METRIC_PERFECT_WINS, 1),
        A("As du carre", "Termine 3 mondes sans mourir.", ACH_METRIC_PERFECT_WINS, 3),
        A("Ramasseur", "Ramasse 50 pieces.", ACH_METRIC_COINS, 50),
        A("Tresor", "Ramasse 250 pieces.", ACH_METRIC_COINS, 250),
        A("Premier diamant", "Ramasse un diamant.", ACH_METRIC_DIAMONDS, 1),
        A("Diamantaire", "Ramasse 10 diamants.", ACH_METRIC_DIAMONDS, 10),
        A("Premiere cle", "Ramasse une cle.", ACH_METRIC_KEYS, 1),
        A("Point de repere", "Atteins 10 checkpoints.", ACH_METRIC_CHECKPOINTS, 10),
        A("Boussole", "Atteins 25 checkpoints.", ACH_METRIC_CHECKPOINTS, 25),
        A("Petit chrono", "Termine 1 monde en moins de 10 minutes.", ACH_METRIC_FAST_WINS, 1),
        A("Fonceur", "Termine 3 mondes en moins de 10 minutes.", ACH_METRIC_FAST_WINS, 3),
    },
};

#undef A

const AchievementDef* achievement_get(int game, int index) {
    if (game < 0 || game >= NUM_GAMES || index < 0 || index >= ACHIEVEMENTS_PER_GAME) return NULL;
    return &s_achievements[game][index];
}

uint32_t achievement_progress(const SaveData* save, int game, int index) {
    const AchievementDef* def = achievement_get(game, index);
    if (!save || !def) return 0;

    switch (def->metric) {
        case ACH_METRIC_WINS:
        case ACH_METRIC_PERFECT_WINS: {
            uint32_t count = 0;
            for (uint16_t i = 0; i < g_games[game].main_levels && i < MAX_LEVELS_PER_GAME; i++) {
                if (save->best_deaths[game][i] >= 0 &&
                    (def->metric == ACH_METRIC_WINS || save->best_deaths[game][i] == 0)) {
                    count++;
                }
            }
            return count;
        }
        case ACH_METRIC_COINS: return save->achievement_coins[game];
        case ACH_METRIC_KEYS: return save->achievement_keys[game];
        case ACH_METRIC_DIAMONDS: return save->achievement_diamonds[game];
        case ACH_METRIC_CHECKPOINTS: return save->achievement_checkpoints[game];
        case ACH_METRIC_FAST_WINS: {
            uint32_t count = 0;
            uint32_t limit = (game == GAME_WHG3 || game == GAME_WHG4) ? 36000u : 3600u;
            for (uint16_t i = 0; i < g_games[game].main_levels && i < MAX_LEVELS_PER_GAME; i++) {
                uint32_t ticks = save->best_ticks[game][i];
                if (ticks > 0 && ticks <= limit) count++;
            }
            return count;
        }
    }
    return 0;
}

uint8_t achievement_percent(const SaveData* save, int game, int index) {
    const AchievementDef* def = achievement_get(game, index);
    if (!def) return 0;
    uint32_t progress = achievement_progress(save, game, index);
    if (progress >= def->target) return 100;
    return (uint8_t)(((uint64_t)progress * 100u) / def->target);
}

bool achievement_is_unlocked(const SaveData* save, int game, int index) {
    if (!save || !achievement_get(game, index)) return false;
    return (save->achievement_unlocked[game] & (1u << index)) != 0;
}

uint8_t achievement_completion_percent(const SaveData* save, int game) {
    if (!save || game < 0 || game >= NUM_GAMES) return 0;
    uint32_t flags = save->achievement_unlocked[game];
    uint8_t unlocked = 0;
    for (int i = 0; i < ACHIEVEMENTS_PER_GAME; i++) {
        if (flags & (1u << i)) unlocked++;
    }
    return (uint8_t)(((uint32_t)unlocked * 100u) / ACHIEVEMENTS_PER_GAME);
}

uint32_t achievement_refresh(SaveData* save, int game) {
    if (!save || game < 0 || game >= NUM_GAMES) return 0;
    uint32_t newly_unlocked = 0;
    for (int i = 0; i < ACHIEVEMENTS_PER_GAME; i++) {
        uint32_t bit = 1u << i;
        const AchievementDef* def = &s_achievements[game][i];
        if (!(save->achievement_unlocked[game] & bit) &&
            achievement_progress(save, game, i) >= def->target) {
            save->achievement_unlocked[game] |= bit;
            newly_unlocked |= bit;
        }
    }
    return newly_unlocked;
}
