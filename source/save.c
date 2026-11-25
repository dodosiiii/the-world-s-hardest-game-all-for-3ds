#include "save.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

void save_defaults(SaveData* sd) {
    memset(sd, 0, sizeof(*sd));
    sd->magic = SAVE_MAGIC;
    sd->version = SAVE_VERSION;
    sd->unlocked_level = 1;
    sd->total_deaths = 0;
    sd->muted = 0;
    sd->player_skin = 0;
    sd->show_timer = 1;
    sd->show_hud = 1;
    for (int i = 0; i < 54; i++) sd->best_deaths[i] = -1;
}

bool save_load(SaveData* sd) {
    save_defaults(sd);
    FILE* f = fopen(SAVE_PATH, "rb");
    if (!f) return false;
    SaveData tmp;
    size_t n = fread(&tmp, 1, sizeof(tmp), f);
    fclose(f);
    if (n != sizeof(tmp) || tmp.magic != SAVE_MAGIC || tmp.version != SAVE_VERSION) {
        return false;
    }
    if (tmp.unlocked_level < 1) tmp.unlocked_level = 1;
    if (tmp.unlocked_level > 50) tmp.unlocked_level = 50;
    if (tmp.player_skin > 5) tmp.player_skin = 0;
    *sd = tmp;
    return true;
}

bool save_write(const SaveData* sd) {
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/whg2", 0777);
    FILE* f = fopen(SAVE_PATH, "wb");
    if (!f) return false;
    size_t n = fwrite(sd, 1, sizeof(*sd), f);
    fclose(f);
    return n == sizeof(*sd);
}
