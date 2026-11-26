#include "save.h"
#include "achievements.h"
#include "language.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/*
 * Chaque version de la sauvegarde est le prefixe exact de la suivante (les champs
 * sont seulement ajoutes a la fin). On lit donc n'importe quelle version dans une
 * structure mise a zero, puis on remplit les champs que l'ancienne version n'avait pas.
 * Avant : une structure "SaveDataVn" recopiee par version, a chaque nouvelle version.
 */
static size_t version_min_size(uint16_t v) {
    switch (v) {
        case 1: return offsetof(SaveData, achievement_unlocked);
        case 2: return offsetof(SaveData, language);
        case 3: return offsetof(SaveData, discovery_mode);
        case 4: return offsetof(SaveData, fx_enabled);
        case 5: return sizeof(SaveData);
        default: return 0;
    }
}

static void normalize_save(SaveData* sd) {
    if (sd->player_skin > 5) sd->player_skin = 0;
    if (sd->speed_mode > 2) sd->speed_mode = 0;
    if (sd->zoom_mode > 2) sd->zoom_mode = 0;
    if (sd->language >= LANG_COUNT && sd->language != LANGUAGE_UNSET)
        sd->language = LANG_FRENCH;
    sd->discovery_mode = sd->discovery_mode ? 1 : 0;
    sd->fx_enabled = sd->fx_enabled ? 1 : 0;
    const uint32_t achievement_mask = (1u << ACHIEVEMENTS_PER_GAME) - 1u;
    for (int g = 0; g < NUM_GAMES; g++) {
        uint16_t max_l = g_games[g].main_levels > 0 ? g_games[g].main_levels : 1;
        if (sd->unlocked_level[g] < 1) sd->unlocked_level[g] = 1;
        if (sd->unlocked_level[g] > max_l) sd->unlocked_level[g] = max_l;
        sd->achievement_unlocked[g] &= achievement_mask;
    }
}

void save_defaults(SaveData* sd) {
    memset(sd, 0, sizeof(*sd));
    sd->magic = SAVE_MAGIC;
    sd->version = SAVE_VERSION;
    sd->muted = 0;
    sd->player_skin = 0;
    sd->show_timer = 1;
    sd->show_hud = 1;
    sd->speed_mode = 0;
    sd->zoom_mode = 0;
    sd->language = LANG_FRENCH;
    sd->discovery_mode = 0;
    sd->fx_enabled = 1;
    for (int g = 0; g < NUM_GAMES; g++) {
        sd->unlocked_level[g] = 1;
        sd->total_deaths[g] = 0;
        for (int i = 0; i < MAX_LEVELS_PER_GAME; i++) {
            sd->best_deaths[g][i] = -1;
        }
    }
}

static bool load_file(const char* path, SaveData* out) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    SaveData tmp;
    memset(&tmp, 0, sizeof(tmp));
    size_t n = fread(&tmp, 1, sizeof(tmp), f);
    fclose(f);

    if (n < sizeof(tmp.magic) + sizeof(tmp.version) || tmp.magic != SAVE_MAGIC) return false;
    uint16_t v = tmp.version;
    size_t need = version_min_size(v);
    if (need == 0 || n < need) return false;

    /* champs absents de l'ancienne version : valeurs par defaut explicites */
    if (v < 3) tmp.language = LANGUAGE_UNSET;     /* l'ecran de choix de langue s'affichera */
    if (v < 4) tmp.discovery_mode = 0;
    if (v < 5) {
        tmp.fx_enabled = 1;
        memset(tmp.best_ticks, 0, sizeof(tmp.best_ticks));
    }
    tmp.version = SAVE_VERSION;
    normalize_save(&tmp);
    *out = tmp;
    return true;
}

bool save_load(SaveData* sd) {
    save_defaults(sd);
    if (load_file(SAVE_PATH, sd)) return true;
    /* fichier principal absent OU corrompu (coupure pendant le renommage) : on tente le temporaire.
     * Avant : le temporaire n'etait essaye que si le principal n'existait pas du tout. */
    save_defaults(sd);
    if (load_file(SAVE_TMP_PATH, sd)) return true;
    save_defaults(sd);
    return false;
}

bool save_write(const SaveData* sd) {
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/whg_hub", 0777);
    /* Ecriture atomique : fichier temporaire complet, puis remplacement.
     * Une coupure (batterie, fermeture) ne peut plus corrompre la sauvegarde. */
    FILE* f = fopen(SAVE_TMP_PATH, "wb");
    if (!f) return false;
    size_t n = fwrite(sd, 1, sizeof(*sd), f);
    bool ok = (n == sizeof(*sd)) && (fflush(f) == 0);
    fclose(f);
    if (!ok) {
        remove(SAVE_TMP_PATH);
        return false;
    }
    remove(SAVE_PATH);
    if (rename(SAVE_TMP_PATH, SAVE_PATH) != 0) {
        f = fopen(SAVE_PATH, "wb");
        if (!f) return false;
        n = fwrite(sd, 1, sizeof(*sd), f);
        fclose(f);
        remove(SAVE_TMP_PATH);
        return n == sizeof(*sd);
    }
    return true;
}
