#include "game_registry.h"
#include "level.h"
#include "game.h"
#include "render.h"
#include "audio.h"
#include "save.h"
#include "achievements.h"
#include "language.h"

#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef enum {
    SCR_HUB = 0,
    SCR_TITLE,
    SCR_SELECT,
    SCR_ACHIEVEMENTS,
    SCR_TUTORIAL,
    SCR_PLAYING,
    SCR_PAUSE,
    SCR_OPTIONS,
    SCR_LANGUAGE,
    SCR_RESET_CONFIRM,
    SCR_VICTORY,
    SCR_SPLASH
} ScreenState;

static SaveData s_save;
static LevelData s_level;
static LevelData s_preview_level;
static int s_preview_game = -1;
static int s_preview_id = -1;
static GameState s_game;
static bool s_level_loaded = false;
static bool s_is_n3ds = false;

static ScreenState s_screen = SCR_HUB;
static ScreenState s_options_return = SCR_HUB;
static int s_hub_cursor = 0;
static int s_active_game = GAME_WHG1;
static int s_menu_cursor = 0;
static int s_select_id = 1;
static int s_tut_cursor = 0;

static bool s_room_exists[5][5];
static int s_room_scan_game = -1;
static int s_room_scan_world = -1;
static char s_sel_col = 'A';
static uint8_t s_sel_row = 1;
static char s_start_col = 'A';
static uint8_t s_start_row = 1;
static char s_preview_col = 0;
static uint8_t s_preview_row = 0;
static int s_achievement_cursor = 0;
static int s_language_cursor = LANG_FRENCH;
static uint8_t s_language_choice = LANG_FRENCH;
static bool s_reset_save_failed = false;
static int s_achievement_first = 0;       /* premiere ligne visible de la liste des succes */
static int s_options_saved_cursor = 0;    /* curseur a restaurer en quittant les options */
static int s_fade = 0, s_fade_max = 1;    /* fondu depuis le noir (frames restantes) */
static int s_hold = 0, s_hold_max = 1;    /* gel du jeu pendant la carte de titre de niveau */
static double s_acc = 0.0;                /* temps accumule pour le pas fixe de la simulation */
static u64 s_last_tick = 0;
static int s_splash_t = 0;                /* image courante de l'ecran de demarrage */
static ScreenState s_after_splash = SCR_HUB;
static int s_hub_anim = 0;                /* entree echelonnee des cartes du hub */
static int s_deny = 0;                    /* tremblement quand un niveau verrouille est refuse */

/* 1 = seuls les niveaux deja debloques peuvent etre lances (le "Mode sans mort" les ouvre tous).
 * Avant : la grille grisait les niveaux a venir, mais A / un appui les lancait quand meme. */
#define ENFORCE_LEVEL_LOCKS 1
#define SPLASH_FRAMES 240

/* lignes du menu Options : des noms plutot que des numeros (avant : 0..7 disperses dans le code) */
enum { OPT_SOUND = 0, OPT_COLOR, OPT_TIMER, OPT_SPEED, OPT_ZOOM, OPT_LANG, OPT_FX,
       OPT_DISCOVERY, OPT_RESET, OPT_COUNT };
static int options_count(void) { return (s_options_return == SCR_HUB) ? OPT_COUNT : OPT_COUNT - 1; }

static bool level_locked(int id) {
#if ENFORCE_LEVEL_LOCKS
    if (s_save.discovery_mode) return false;
    if (id > (int)g_games[s_active_game].main_levels) return false;   /* tutoriels : toujours libres */
    return id > (int)s_save.unlocked_level[s_active_game];
#else
    (void)id;
    return false;
#endif
}

static void fmt_time(char* out, size_t n, uint32_t ticks) {
    uint32_t ds = (ticks * 10u) / 60u;
    snprintf(out, n, "%lu:%02lu.%lu", (unsigned long)(ds / 600u),
             (unsigned long)((ds / 10u) % 60u), (unsigned long)(ds % 10u));
}
static struct {
    uint8_t game;
    uint8_t index;
} s_achievement_queue[NUM_GAMES * ACHIEVEMENTS_PER_GAME];
static uint8_t s_achievement_queue_head;
static uint8_t s_achievement_queue_count;
static uint16_t s_achievement_toast_frames;
static int s_achievement_toast_game = -1;
static int s_achievement_toast_index = -1;

#define ACHIEVEMENT_VISIBLE_ROWS 4
#define ACHIEVEMENT_TOAST_DURATION 150
/* periode reelle de l'ecran haut de la 3DS (59.83 Hz) : la simulation tourne a cette cadence */
#define STEP_SECONDS (4481136.0 / 268111856.0)

static void queue_achievement_unlocks(int game, uint32_t newly_unlocked) {
    for (int i = 0; i < ACHIEVEMENTS_PER_GAME; i++) {
        if (!(newly_unlocked & (1u << i)) ||
            s_achievement_queue_count >= NUM_GAMES * ACHIEVEMENTS_PER_GAME) continue;
        uint8_t tail = (uint8_t)((s_achievement_queue_head + s_achievement_queue_count) %
                                 (NUM_GAMES * ACHIEVEMENTS_PER_GAME));
        s_achievement_queue[tail].game = (uint8_t)game;
        s_achievement_queue[tail].index = (uint8_t)i;
        s_achievement_queue_count++;
    }
}

static void update_achievement_toast(void) {
    if (s_achievement_toast_frames > 0) {
        s_achievement_toast_frames--;
    } else if (s_achievement_queue_count > 0) {
        s_achievement_toast_game = s_achievement_queue[s_achievement_queue_head].game;
        s_achievement_toast_index = s_achievement_queue[s_achievement_queue_head].index;
        s_achievement_queue_head = (uint8_t)((s_achievement_queue_head + 1) %
                                             (NUM_GAMES * ACHIEVEMENTS_PER_GAME));
        s_achievement_queue_count--;
        s_achievement_toast_frames = ACHIEVEMENT_TOAST_DURATION;
        static const SfxId jingles[3] = { SFX_DIAMOND, SFX_CHECK, SFX_KEY };
        for (int i = 0; i < 3; i++) {
            if (audio_has_sfx(jingles[i])) { audio_play_sfx(jingles[i]); break; }
        }
    }
}

static uint32_t count_collected_keys(const GameState* gs) {
    uint32_t count = 0;
    if (gs->is_whg3 || gs->is_whg4) {
        for (int i = 0; i < MAX_DOORS; i++) {
            if (gs->whg3_key_collected[i]) count++;
        }
    } else if (gs->key_collected) {
        for (uint16_t i = 0; i < gs->lvl->num_keys; i++) {
            if (gs->key_collected[i]) count++;
        }
    }
    return count;
}

static void add_achievement_progress(uint32_t* progress, uint32_t amount) {
    if (UINT32_MAX - *progress < amount) *progress = UINT32_MAX;
    else *progress += amount;
}

static bool refresh_game_achievements(int game, bool notify) {
    uint32_t newly_unlocked = achievement_refresh(&s_save, game);
    if (newly_unlocked == 0) return false;
    if (notify) queue_achievement_unlocks(game, newly_unlocked);
    return true;
}

static float get_speed_multiplier(void) {
    if (s_save.speed_mode == 1) return 1.25f;
    if (s_save.speed_mode == 2) return 1.50f;
    return 1.00f;
}

static const char* get_speed_label(void) {
    if (s_save.speed_mode == 1) return "Rapide  x1.25";
    if (s_save.speed_mode == 2) return "Turbo  x1.5";
    return "Normale  x1.0";
}

static float get_zoom_factor(void) {
    if (s_save.zoom_mode == 1) return 1.50f;
    if (s_save.zoom_mode == 2) return 2.00f;
    return 1.00f;
}

static const char* get_zoom_label(void) {
    if (s_save.zoom_mode == 1) return "Suivi  x1.5";
    if (s_save.zoom_mode == 2) return "Proche  x2.0";
    return "Global  x1.0";
}

/* ---------- transitions, options, defilement des succes ---------- */

static void begin_fade(int frames) {
    s_fade = frames;
    s_fade_max = frames > 0 ? frames : 1;
}

/* carte de titre + fondu quand un niveau / monde demarre */
static void begin_level_intro(int hold_frames) {
    s_hold = hold_frames;
    s_hold_max = hold_frames > 0 ? hold_frames : 1;
    begin_fade(20);
    render_camera_reset();
    render_fx_clear();
    s_acc = 0.0;
}

static void open_options(ScreenState from) {
    s_options_return = from;
    s_options_saved_cursor = s_menu_cursor;
    s_screen = SCR_OPTIONS;
    s_menu_cursor = 0;
}

static void close_options(void) {
    save_write(&s_save);
    s_screen = s_options_return;
    s_menu_cursor = s_options_saved_cursor;   /* avant : curseur hors liste en revenant (rien de selectionne) */
}

static void achievement_scroll_fix(void) {
    int max_first = ACHIEVEMENTS_PER_GAME - ACHIEVEMENT_VISIBLE_ROWS;
    if (s_achievement_cursor < s_achievement_first) s_achievement_first = s_achievement_cursor;
    if (s_achievement_cursor >= s_achievement_first + ACHIEVEMENT_VISIBLE_ROWS)
        s_achievement_first = s_achievement_cursor - ACHIEVEMENT_VISIBLE_ROWS + 1;
    if (s_achievement_first > max_first) s_achievement_first = max_first;
    if (s_achievement_first < 0) s_achievement_first = 0;
}

static void scan_world_rooms(int game, int world) {
    if (game == s_room_scan_game && world == s_room_scan_world) return;
    s_room_scan_game = game;
    s_room_scan_world = world;
    for (int c = 0; c < 5; c++) {
        for (int r = 0; r < 5; r++) {
            char path[96];
            snprintf(path, sizeof(path), "%s/levels/l%d_%c%d.lvl",
                     g_games[game].romfs_prefix, world, 'A' + c, r + 1);
            FILE* f = fopen(path, "rb");
            s_room_exists[c][r] = (f != NULL);
            if (f) fclose(f);
        }
    }
    if (game == GAME_WHG3) {
        s_sel_col = WHG3_STARTS[world - 1].col;
        s_sel_row = WHG3_STARTS[world - 1].row;
    } else if (game == GAME_WHG4) {
        s_sel_col = WHG4_STARTS[world - 1].col;
        s_sel_row = WHG4_STARTS[world - 1].row;
    }
}

static void load_preview_room(int id, char col, uint8_t row) {
    if (id == s_preview_id && s_active_game == s_preview_game &&
        col == s_preview_col && row == s_preview_row) return;
    level_free(&s_preview_level);
    char path[96];
    if (s_active_game == GAME_WHG3 || s_active_game == GAME_WHG4) {
        snprintf(path, sizeof(path), "%s/levels/l%d_%c%u.lvl",
                 g_games[s_active_game].romfs_prefix, id, col, row);
    } else {
        snprintf(path, sizeof(path), "%s/levels/%02d.lvl", g_games[s_active_game].romfs_prefix, id);
    }
    if (level_load(&s_preview_level, path)) {
        s_preview_id = id;
        s_preview_game = s_active_game;
        s_preview_col = col;
        s_preview_row = row;
    } else {
        s_preview_id = -1;
        s_preview_game = -1;
    }
}

static void load_preview(int id) {
    if (s_active_game == GAME_WHG3) {
        const Whg3StartFinish* st = &WHG3_STARTS[id - 1];
        load_preview_room(id, st->col, st->row);
    } else if (s_active_game == GAME_WHG4) {
        const Whg4StartFinish* st = &WHG4_STARTS[id - 1];
        load_preview_room(id, st->col, st->row);
    } else {
        load_preview_room(id, 0, 0);
    }
}

static bool load_whg3_room(int level_num, char col, uint8_t row) {
    LevelData next = {0};
    char path[96];
    snprintf(path, sizeof(path), "%s/levels/l%d_%c%u.lvl", g_games[GAME_WHG3].romfs_prefix, level_num, col, row);
    if (!level_load(&next, path)) return false;
    level_free(&s_level);
    s_level = next;
    s_level_loaded = true;
    return true;
}

static bool load_whg4_room(int level_num, char col, uint8_t row) {
    LevelData next = {0};
    char path[96];
    snprintf(path, sizeof(path), "%s/levels/l%d_%c%u.lvl", g_games[GAME_WHG4].romfs_prefix, level_num, col, row);
    if (!level_load(&next, path)) return false;
    level_free(&s_level);
    s_level = next;
    s_level_loaded = true;
    return true;
}

static bool start_level_at_room(int id, char col, uint8_t row) {
    LevelData next = {0};
    char path[96];

    if (s_active_game == GAME_WHG3) {
        snprintf(path, sizeof(path), "%s/levels/l%d_%c%u.lvl", g_games[GAME_WHG3].romfs_prefix, id, col, row);
        if (!level_load(&next, path)) return false;
    } else if (s_active_game == GAME_WHG4) {
        snprintf(path, sizeof(path), "%s/levels/l%d_%c%u.lvl", g_games[GAME_WHG4].romfs_prefix, id, col, row);
        if (!level_load(&next, path)) return false;
    } else {
        return false;
    }

    game_free(&s_game);
    level_free(&s_level);
    s_level = next;
    if (s_active_game == GAME_WHG3) {
        game_init_whg3_at(&s_game, (uint8_t)id, &s_level, col, row, s_save.total_deaths[s_active_game]);
        s_start_col = col;
        s_start_row = row;
    } else {
        game_init_whg4_at(&s_game, (uint8_t)id, &s_level, col, row, s_save.total_deaths[s_active_game]);
        s_start_col = col;
        s_start_row = row;
    }

    s_game.invincible = s_save.discovery_mode != 0;
    s_level_loaded = true;
    s_screen = SCR_PLAYING;
    begin_level_intro(24);
    return true;
}

static bool start_level(int id) {
    LevelData next = {0};
    char path[96];

    if (s_active_game == GAME_WHG3) {
        const Whg3StartFinish* st = &WHG3_STARTS[id - 1];
        snprintf(path, sizeof(path), "%s/levels/l%d_%c%u.lvl",
                 g_games[GAME_WHG3].romfs_prefix, id, st->col, st->row);
    } else if (s_active_game == GAME_WHG4) {
        const Whg4StartFinish* st = &WHG4_STARTS[id - 1];
        snprintf(path, sizeof(path), "%s/levels/l%d_%c%u.lvl",
                 g_games[GAME_WHG4].romfs_prefix, id, st->col, st->row);
    } else {
        snprintf(path, sizeof(path), "%s/levels/%02d.lvl", g_games[s_active_game].romfs_prefix, id);
    }
    if (!level_load(&next, path)) return false;

    game_free(&s_game);
    level_free(&s_level);
    s_level = next;
    if (s_active_game == GAME_WHG3) {
        game_init_whg3(&s_game, (uint8_t)id, &s_level, s_save.total_deaths[s_active_game]);
        s_start_col = WHG3_STARTS[id - 1].col;
        s_start_row = WHG3_STARTS[id - 1].row;
    } else if (s_active_game == GAME_WHG4) {
        game_init_whg4(&s_game, (uint8_t)id, &s_level, s_save.total_deaths[s_active_game]);
        s_start_col = WHG4_STARTS[id - 1].col;
        s_start_row = WHG4_STARTS[id - 1].row;
    } else {
        if (!game_init(&s_game, &s_level, s_save.total_deaths[s_active_game])) {
            level_free(&s_level);
            s_level_loaded = false;
            s_screen = SCR_TITLE;
            return false;
        }
    }

    s_game.invincible = s_save.discovery_mode != 0;
    s_level_loaded = true;
    s_screen = SCR_PLAYING;
    begin_level_intro(24);
    return true;
}

static void deny_start(void) {
    s_deny = 14;
    audio_play_sfx(SFX_PAUSE);
}

static bool try_start_level(int id) {
    if (level_locked(id)) { deny_start(); return false; }
    return start_level(id);
}

static bool try_start_room(int id, char col, uint8_t row) {
    if (level_locked(id)) { deny_start(); return false; }
    return start_level_at_room(id, col, row);
}

static bool restart_current_level(void) {
    if (s_game.is_whg3 || s_game.is_whg4) {
        bool ok = start_level_at_room(s_game.cur_level, s_start_col, s_start_row);
        if (ok) { s_hold = 0; begin_fade(6); }   /* recommencer : pas de carte de titre */
        return ok;
    }
    game_restart_level(&s_game);
    render_camera_reset();
    render_fx_clear();
    begin_fade(6);
    s_screen = SCR_PLAYING;
    return true;
}

static void enter_game(int game_idx) {
    if (game_idx < 0 || game_idx >= NUM_GAMES || !g_games[game_idx].available) return;
    s_active_game = game_idx;
    s_select_id = s_save.unlocked_level[game_idx];
    s_menu_cursor = 0;
    s_tut_cursor = 0;
    audio_load_bank(g_games[game_idx].romfs_prefix);
    load_preview(s_select_id);
    s_screen = SCR_TITLE;
    begin_fade(14);
}

static void return_to_hub(void) {
    save_write(&s_save);
    audio_unload_bank();
    render_fx_clear();
    s_hub_anim = 0;
    s_screen = SCR_HUB;
    begin_fade(10);
}

static bool reset_all_progress(void) {
    SaveData previous = s_save;
    SaveData reset;
    save_defaults(&reset);
    reset.muted = previous.muted;
    reset.player_skin = previous.player_skin;
    reset.show_timer = previous.show_timer;
    reset.show_hud = previous.show_hud;
    reset.speed_mode = previous.speed_mode;
    reset.zoom_mode = previous.zoom_mode;
    reset.language = previous.language;
    reset.discovery_mode = previous.discovery_mode;
    reset.fx_enabled = previous.fx_enabled;
    s_save = reset;
    if (!save_write(&s_save)) {
        s_save = previous;
        return false;
    }
    return true;
}

static GameInput read_movement_input(u32 kHeld) {
    GameInput in = { 0, 0 };
    circlePosition cpad;
    hidCircleRead(&cpad);

    if ((kHeld & KEY_RIGHT) || cpad.dx > 28) in.dx += 1;
    if ((kHeld & KEY_LEFT)  || cpad.dx < -28) in.dx -= 1;
    if ((kHeld & KEY_UP)    || cpad.dy > 28) in.dy -= 1;
    if ((kHeld & KEY_DOWN)  || cpad.dy < -28) in.dy += 1;
    return in;
}

/* =====================================================================
 *  UI : design system plat (palette, primitives, mise en page, ecrans)
 *  Une seule couleur d'accent, surfaces sombres neutres, pas de bordures
 *  partout : la hierarchie vient des niveaux de gris et de l'espacement.
 * ===================================================================== */

#define UZ 0.0f   /* meme profondeur pour toutes les formes UI : ordre de dessin */

typedef struct {
    u32 bg, checker, surf, surf2, surfHi, line, track;
    u32 text, dim, muted;
    u32 accent, accentHi;
    u32 good, goodZone, blue, black, scrim, scrimDark;
} UiTheme;

typedef struct { const char* key; const char* label; } UiHint;

static UiTheme T;
static uint32_t s_frame = 0;

static void ui_init(void) {
    T.bg        = C2D_Color32(0x0E, 0x10, 0x14, 0xFF);
    T.checker   = C2D_Color32(0x15, 0x18, 0x1E, 0xFF);
    T.surf      = C2D_Color32(0x1A, 0x1D, 0x25, 0xFF);
    T.surf2     = C2D_Color32(0x24, 0x28, 0x32, 0xFF);
    T.surfHi    = C2D_Color32(0x31, 0x36, 0x43, 0xFF);
    T.line      = C2D_Color32(0x2B, 0x30, 0x3B, 0xFF);
    T.track     = C2D_Color32(0x3A, 0x40, 0x4E, 0xFF);
    T.text      = C2D_Color32(0xF2, 0xF4, 0xF8, 0xFF);
    T.dim       = C2D_Color32(0xA4, 0xAB, 0xB9, 0xFF);
    T.muted     = C2D_Color32(0x6C, 0x74, 0x85, 0xFF);
    T.accent    = C2D_Color32(0xE5, 0x48, 0x4D, 0xFF);
    T.accentHi  = C2D_Color32(0xFF, 0x62, 0x67, 0xFF);
    T.good      = C2D_Color32(0x3D, 0xC2, 0x8A, 0xFF);
    T.goodZone  = C2D_Color32(0x16, 0x2F, 0x28, 0xFF);
    T.blue      = C2D_Color32(0x3A, 0x5C, 0xF2, 0xFF);
    T.black     = C2D_Color32(0x05, 0x06, 0x08, 0xFF);
    T.scrim     = C2D_Color32(0x0A, 0x0B, 0x0F, 0xE6);
    T.scrimDark = C2D_Color32(0x05, 0x06, 0x09, 0xB8);
}

/* ---------- primitives ---------- */

static inline void ui_rect(float x, float y, float w, float h, u32 c) {
    C2D_DrawRectSolid(x, y, UZ, w, h, c);
}

/* rectangle a coins arrondis (2 px, en escalier : net sur l'ecran de la 3DS) */
static void ui_rrect(float x, float y, float w, float h, u32 c) {
    ui_rect(x + 2, y,     w - 4, h,     c);
    ui_rect(x + 1, y + 1, w - 2, h - 2, c);
    ui_rect(x,     y + 2, w,     h - 4, c);
}

static void ui_ring(float x, float y, float w, float h, float t, u32 outline, u32 fill) {
    ui_rrect(x, y, w, h, outline);
    ui_rect(x + t, y + t, w - 2 * t, h - 2 * t, fill);
}

static inline void ui_dot(float cx, float cy, float r, u32 c) {
    C2D_DrawCircleSolid(cx, cy, UZ, r, c);
}

static void ui_chevron(float cx, float cy, float r, bool left, u32 c) {
    float dx = left ? -r : r;
    C2D_DrawLine(cx - dx * 0.5f, cy - r, c, cx + dx * 0.5f, cy, c, 2.0f, UZ);
    C2D_DrawLine(cx + dx * 0.5f, cy, c, cx - dx * 0.5f, cy + r, c, 2.0f, UZ);
}

static inline float ui_midy(float y, float h, float s) {
    return (float)(int)(y + (h - 30.0f * s) * 0.5f);
}
static uint8_t ui_language(void) {
    return s_screen == SCR_LANGUAGE ? s_language_choice : s_save.language;
}
static inline void ui_text(float x, float y, float s, u32 c, const char* t) {
    render_draw_text(x, y, s, c, language_text(ui_language(), t));
}
static inline void ui_text_c(float cx, float y, float s, u32 c, const char* t) {
    render_draw_text_centered(cx, y, s, c, language_text(ui_language(), t));
}

/* ---------- mise en page (partagee entre update et render) ---------- */

static UiButton lay_back(void)        { return (UiButton){ 8, 4, 66, 24, NULL }; }
static UiButton lay_hub_options(void) { return (UiButton){ 238, 4, 74, 24, NULL }; }
static UiButton lay_hub_card(int i)   { return (UiButton){ 8 + (i % 2) * 156, 40 + (i / 2) * 86, 148, 78, NULL }; }
static UiButton lay_play(void)        { return (UiButton){ 228, 215, 84, 22, NULL }; }
static UiButton lay_world_tab(int w)  { return (UiButton){ 8 + w * 62, 40, 56, 24, NULL }; }
static UiButton lay_room(int c, int r){ return (UiButton){ 18 + c * 58, 72 + r * 26, 52, 22, NULL }; }
static UiButton lay_achievement_row(int row) { return (UiButton){ 8, 40 + row * 41, 304, 37, NULL }; }
static UiButton lay_opt_row(int i)    { return (UiButton){ 16, 38 + i * 19, 288, 17, NULL }; }
static UiButton lay_hud_btn(int i)    { return (UiButton){ 12 + i * 100, 200, 96, 32, NULL }; }

static UiButton lay_menu_row(int i, int n) {
    int h = (n > 4) ? 28 : 36;
    int gap = (n > 4) ? 4 : 6;
    int total = n * h + (n - 1) * gap;
    int y0 = 38 + (170 - total) / 2;
    return (UiButton){ 16, y0 + i * (h + gap), 288, h, NULL };
}

static UiButton lay_grid_cell(int i, int max_lvl) {
    int cols = (max_lvl <= 30) ? 6 : 10;
    int rows = (max_lvl + cols - 1) / cols;
    int c = i % cols, r = i / cols;
    int pitch_y = 164 / rows;
    if (pitch_y > 36) pitch_y = 36;
    int ch = pitch_y - 5;
    int cw = (cols == 6) ? 40 : 28;
    int pitch_x = (cols == 6) ? 46 : 30;
    int total_w = (cols - 1) * pitch_x + cw;
    int total_h = (rows - 1) * pitch_y + ch;
    int x0 = ((int)BOT_W - total_w) / 2;
    int y0 = 40 + (168 - total_h) / 2;
    return (UiButton){ x0 + c * pitch_x, y0 + r * pitch_y, cw, ch, NULL };
}

/* ---------- composants bas d'ecran ---------- */

static float ui_keycap(float x, float y, const char* key) {
    float w = render_text_width(0.36f, key) + 10.0f;
    if (w < 16.0f) w = 16.0f;
    ui_rrect(x, y + 1, w, 16, T.bg);
    ui_rrect(x, y, w, 16, T.surfHi);
    ui_text_c(x + w * 0.5f, ui_midy(y, 16.0f, 0.36f), 0.36f, T.text, key);
    return w;
}

static void ui_footer(const UiHint* h, int n) {
    ui_rect(0, 212, BOT_W, 28, T.surf);
    ui_rect(0, 212, BOT_W, 1, T.line);
    float x = 12.0f;
    for (int i = 0; i < n; i++) {
        /* largeurs mesurees (avant : estimation par nombre de lettres, debordait en ES/IT) */
        const char* label = language_text(ui_language(), h[i].label);
        float kw = ui_keycap(x, 218.0f, h[i].key);
        render_draw_text(x + kw + 6.0f, ui_midy(212.0f, 28.0f, 0.4f), 0.4f, T.dim, label);
        x += kw + 6.0f + render_text_width(0.4f, label) + 16.0f;
    }
}

static void ui_header(const char* title, const char* back_label, const char* action_label) {
    ui_rect(0, 0, BOT_W, 32, T.surf);
    ui_rect(0, 32, BOT_W, 1, T.line);
    if (back_label) {
        UiButton b = lay_back();
        ui_rrect(b.x, b.y, b.w, b.h, T.surf2);
        ui_chevron(b.x + 13.0f, b.y + b.h * 0.5f, 5.0f, true, T.dim);
        ui_text(b.x + 24.0f, ui_midy(b.y, b.h, 0.42f), 0.42f, T.text, back_label);
        ui_text_c(BOT_W * 0.5f, ui_midy(0, 32, 0.54f), 0.54f, T.text, title);
    } else {
        ui_text(14.0f, ui_midy(0, 32, 0.56f), 0.56f, T.text, title);
    }
    if (action_label) {
        UiButton o = lay_hub_options();
        ui_rrect(o.x, o.y, o.w, o.h, T.surf2);
        ui_text_c(o.x + o.w * 0.5f, ui_midy(o.y, o.h, 0.42f), 0.42f, T.text, action_label);
    }
}

static void ui_menu_row(UiButton b, const char* label, const char* hint, bool sel, bool primary) {
    float x = b.x, y = b.y, w = b.w, h = b.h;
    u32 fill = primary ? (sel ? T.accentHi : T.accent) : (sel ? T.surfHi : T.surf);
    ui_rrect(x, y, w, h, fill);
    if (sel) ui_rect(x, y + 5, 3, h - 10, primary ? T.text : T.accent);
    ui_text(x + 18, ui_midy(y, h, 0.54f), 0.54f, T.text, label);
    if (hint && hint[0]) ui_text(x + w - 92, ui_midy(y, h, 0.4f), 0.4f, primary ? T.text : T.dim, hint);
    ui_chevron(x + w - 16, y + h * 0.5f, 5.0f, false, primary ? T.text : (sel ? T.accent : T.muted));
}

static void ui_game_card(UiButton b, const GameDef* g, int idx, bool sel) {
    float x = b.x, y = b.y, w = b.w, h = b.h;
    u32 fill = !g->available ? T.surf : (sel ? T.surfHi : T.surf2);
    if (sel) ui_ring(x, y, w, h, 2.0f, g->available ? T.accent : T.muted, fill);
    else     ui_rrect(x, y, w, h, fill);
    ui_text(x + 14, y + 12, 0.58f, g->available ? T.text : T.muted, g->short_name);
    if (g->available) {
        char buf[32];
        snprintf(buf, sizeof(buf), language_text(ui_language(), "Niveau %u / %u"),
                 (unsigned)s_save.unlocked_level[idx], (unsigned)g->main_levels);
        ui_text(x + 14, y + 32, 0.36f, T.dim, buf);
        snprintf(buf, sizeof(buf), language_text(ui_language(), "Succes %u%%"),
                 achievement_completion_percent(&s_save, idx));
        ui_text(x + 14, y + 46, 0.36f, T.good, buf);
        float bw = w - 28.0f;
        float ratio = (g->main_levels > 0) ? (float)s_save.unlocked_level[idx] / (float)g->main_levels : 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        ui_rect(x + 14, y + h - 8, bw, 3, T.track);
        if (ratio > 0.0f) ui_rect(x + 14, y + h - 8, (float)(int)(bw * ratio), 3, T.accent);
    } else {
        ui_text(x + 14, y + 36, 0.4f, T.muted, "Bientot disponible");
    }
}

static void ui_stat_tile(float x, float y, float w, float h, const char* label,
                         const char* value, u32 vcol, float ratio, u32 barcol) {
    ui_rrect(x, y, w, h, T.surf);
    ui_text(x + 10, y + 7, 0.36f, T.muted, label);
    float vs = 0.72f;   /* la valeur est reduite jusqu'a tenir dans la tuile ("$2705 / $2705" debordait) */
    while (vs > 0.36f && render_text_width(vs, value) > w - 18.0f) vs -= 0.04f;
    ui_text(x + 10, y + 19 + (0.72f - vs) * 14.0f, vs, vcol, value);
    if (ratio >= 0.0f) {
        float bw = w - 20.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        ui_rect(x + 10, y + h - 7, bw, 3, T.track);
        if (ratio > 0.0f) ui_rect(x + 10, y + h - 7, (float)(int)(bw * ratio), 3, barcol);
    }
}

static void ui_toggle(float x, float y, bool on) {
    ui_rrect(x, y, 30, 14, on ? T.good : T.track);
    ui_dot(on ? x + 22.0f : x + 8.0f, y + 7.0f, 5.0f, on ? T.text : T.dim);
}

static void ui_dots(float right, float cy, int n, int active, bool cumulative) {
    for (int i = 0; i < n; i++) {
        float cx = right - (float)(n - 1 - i) * 10.0f - 3.0f;
        bool on = cumulative ? (i <= active) : (i == active);
        ui_dot(cx, cy, 3.0f, on ? T.accent : T.track);
    }
}

/* ---------- composants haut d'ecran ---------- */

/* damier qui defile lentement : clin d'oeil au sol du jeu */
static void ui_checker(float w, float h, uint32_t frame) {
    const int S = 20;
    int off = (int)((frame >> 2) % (2 * S));
    ui_rect(0, 0, w, h, T.bg);
    for (int ty = 0; ty * S + off - 2 * S < (int)h; ty++) {
        for (int tx = 0; tx * S + off - 2 * S < (int)w; tx++) {
            if (((tx + ty) & 1) == 0)
                ui_rect((float)(tx * S + off - 2 * S), (float)(ty * S + off - 2 * S), (float)S, (float)S, T.checker);
        }
    }
}

/* mini decor : couloir avec zones de depart/arrivee, cube rouge, ronds bleus */
static void ui_lane(float y, uint32_t frame, float player_x) {
    const float h = 40.0f;
    ui_rect(0, y, TOP_W, h, T.surf);
    ui_rect(0, y + 1, 64, h - 2, T.goodZone);
    ui_rect(TOP_W - 64, y + 1, 64, h - 2, T.goodZone);
    ui_rect(0, y, TOP_W, 1, T.line);
    ui_rect(0, y + h - 1, TOP_W, 1, T.line);
    float t = (float)frame / 60.0f;
    for (int i = 0; i < 4; i++) {
        float cx = 112.0f + (float)i * 58.0f;
        float cy = y + h * 0.5f + sinf(t * 2.4f + (float)i * 1.6f) * 12.0f;
        ui_dot(cx, cy, 7.0f, T.black);
        ui_dot(cx, cy, 5.5f, T.blue);
    }
    ui_rect(player_x, y + 12, 16, 16, T.black);
    ui_rect(player_x + 2, y + 14, 12, 12, render_skin_color(s_save.player_skin));
}

static void ui_status_line(void) {
    ui_text(28, 222, 0.34f, T.muted, s_is_n3ds ? "New 3DS  -  804 MHz" : "Old 3DS  -  268 MHz");
    if (!audio_is_dsp_available())
        ui_text(160, 222, 0.34f, T.accentHi, "Son indisponible : dspfirm.cdc manquant");
}

static void ui_top_menu(const GameDef* g, int idx) {
    const float X = 28.0f;
    char b[32];
    ui_checker(TOP_W, TOP_H, s_frame);
    ui_text(X, 34, 0.44f, T.accent, "THE WORLD'S");
    ui_text(X, 50, 0.92f, g->available ? T.text : T.muted, g->title);
    ui_text(X, 86, 0.5f, T.dim, g->subtitle);
    if (g->available) {
        ui_text(X, 114, 0.36f, T.muted, "PROGRESSION");
        snprintf(b, sizeof(b), "%u / %u", (unsigned)s_save.unlocked_level[idx], (unsigned)g->main_levels);
        ui_text(X, 126, 0.78f, T.text, b);
        float ratio = (g->main_levels > 0) ? (float)s_save.unlocked_level[idx] / (float)g->main_levels : 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        ui_rect(X, 156, 120, 3, T.track);
        if (ratio > 0.0f) ui_rect(X, 156, (float)(int)(120.0f * ratio), 3, T.accent);
        ui_text(X + 160, 114, 0.36f, T.muted, "MORTS");
        snprintf(b, sizeof(b), "%lu", (unsigned long)s_save.total_deaths[idx]);
        ui_text(X + 160, 126, 0.78f, T.text, b);
    } else {
        ui_rrect(X, 118, 132, 24, T.surf2);
        ui_text_c(X + 66, ui_midy(118, 24, 0.42f), 0.42f, T.dim, "Bientot disponible");
    }
    ui_lane(168, s_frame, 24);
    ui_status_line();
}

/* bandeau au-dessus de l'apercu de niveau (profondeur 0.8 comme avant) */
static void ui_top_bar(const char* left, const char* right, u32 right_col) {
    C2D_DrawRectSolid(0, 0, 0.8f, TOP_W, 26, T.scrim);
    C2D_DrawRectSolid(0, 26, 0.8f, TOP_W, 1, T.accent);
    /* profondeur 0.9 : le texte etait a 0.5, donc cache derriere le voile (0.8) */
    render_draw_text_depth(12, ui_midy(0, 26, 0.52f), 0.52f, T.text, 0.9f, left);
    if (right) {
        float rw = render_text_width(0.42f, right);
        render_draw_text_depth(TOP_W - 12.0f - rw, ui_midy(0, 26, 0.42f), 0.42f, right_col, 0.9f, right);
    }
}

static void draw_fade(float w, float h) {
    if (s_fade <= 0) return;
    float a = (float)s_fade / (float)s_fade_max;
    a *= a;
    C2D_DrawRectSolid(0, 0, 0.99f, w, h, C2D_Color32(0, 0, 0, (u8)(a * 255.0f)));
}

/* carte de titre au debut d'un niveau / monde */
static void draw_level_banner(void) {
    if (s_hold <= 0) return;
    const GameDef* cg = &g_games[s_active_game];
    char title[48];
    if (s_game.is_whg3 || s_game.is_whg4) {
        snprintf(title, sizeof(title), language_text(ui_language(), "Monde %d"), (int)s_game.cur_level);
    } else if (s_level.id <= cg->main_levels) {
        snprintf(title, sizeof(title), language_text(ui_language(), "Niveau %u"), (unsigned)s_level.id);
    } else {
        snprintf(title, sizeof(title), language_text(ui_language(), "Tutoriel %u / %u"),
                 (unsigned)(s_level.id - cg->main_levels), (unsigned)cg->tutorial_levels);
    }
    float a = (s_hold >= 14) ? 1.0f : (float)s_hold / 14.0f;
    u8 a255 = (u8)(a * 255.0f);
    C2D_DrawRectSolid(0, 88, 0.85f, TOP_W, 64, C2D_Color32(0x05, 0x06, 0x09, (u8)(a * 215.0f)));
    C2D_DrawRectSolid(0, 88, 0.86f, TOP_W, 2, C2D_Color32(0xE5, 0x48, 0x4D, a255));
    C2D_DrawRectSolid(0, 150, 0.86f, TOP_W, 2, C2D_Color32(0xE5, 0x48, 0x4D, a255));
    render_draw_text_centered_depth(TOP_W * 0.5f, 96.0f, 0.95f, C2D_Color32(0xF2, 0xF4, 0xF8, a255), 0.9f, title);
    render_draw_text_centered_depth(TOP_W * 0.5f, 128.0f, 0.42f, C2D_Color32(0xA4, 0xAB, 0xB9, a255), 0.9f,
                                    language_text(ui_language(), cg->title));
}

static void draw_achievement_toast(void) {
    if (s_achievement_toast_frames == 0) return;
    const AchievementDef* def = achievement_get(s_achievement_toast_game, s_achievement_toast_index);
    if (!def) return;

    /* Bandeau compact en haut au centre qui glisse (avant : 310x82 au milieu de l'ecran,
     * il cachait le joueur et les ennemis pendant 3 secondes). */
    const float W = 272.0f, H = 48.0f;
    const int slide = 14;
    int left = s_achievement_toast_frames;
    int elapsed = ACHIEVEMENT_TOAST_DURATION - left;
    float t = 1.0f;
    if (elapsed < slide) {
        float k = (float)elapsed / (float)slide;
        t = 1.0f - (1.0f - k) * (1.0f - k);
    } else if (left < slide) {
        float k = (float)left / (float)slide;
        t = k * k * (3.0f - 2.0f * k);
    }
    float x = (TOP_W - W) * 0.5f;
    float y = -H + (H + 8.0f) * t;

    const u32 gold   = C2D_Color32(0xF5, 0xB9, 0x2E, 0xFF);
    const u32 goldHi = C2D_Color32(0xFF, 0xD9, 0x6B, 0xFF);
    const u32 ink    = C2D_Color32(0x5A, 0x3D, 0x0A, 0xFF);

    C2D_DrawRectSolid(x + 2.0f, y + 3.0f, 0.88f, W, H, C2D_Color32(0, 0, 0, 120));
    C2D_DrawRectSolid(x, y, 0.89f, W, H, C2D_Color32(0x1A, 0x1D, 0x25, 0xF4));
    C2D_DrawRectSolid(x, y, 0.9f, 3.0f, H, T.good);

    float bx = x + 27.0f, by = y + H * 0.5f;
    C2D_DrawCircleSolid(bx, by, 0.91f, 14.0f, gold);
    C2D_DrawCircleSolid(bx, by, 0.92f, 10.5f, goldHi);
    C2D_DrawLine(bx - 5.0f, by + 0.5f, ink, bx - 1.5f, by + 4.0f, ink, 2.5f, 0.93f);
    C2D_DrawLine(bx - 1.5f, by + 4.0f, ink, bx + 5.5f, by - 4.5f, ink, 2.5f, 0.93f);

    render_draw_text_depth(x + 50.0f, y + 4.0f,  0.34f, T.good, 0.96f,
                           language_text(ui_language(), "SUCCES DEBLOQUE"));
    render_draw_text_depth(x + 50.0f, y + 14.0f, 0.5f,  T.text, 0.96f,
                           language_text(ui_language(), def->name));
    render_draw_text_depth(x + 50.0f, y + 31.0f, 0.36f, T.dim,  0.96f,
                           language_text(ui_language(), def->description));

    float frac = (float)left / (float)ACHIEVEMENT_TOAST_DURATION;
    C2D_DrawRectSolid(x + 3.0f, y + H - 2.0f, 0.9f, (W - 3.0f) * frac, 2.0f, T.good);
}

/* ---------- ecran de demarrage anime ---------- */

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static float ease_out_cubic(float x) { x = 1.0f - x; return 1.0f - x * x * x; }
static float ease_out_back(float x) {
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    float t = x - 1.0f;
    return 1.0f + c3 * t * t * t + c1 * t * t;
}
static u32 with_alpha(u32 c, float a) {
    return (c & 0x00FFFFFFu) | ((u32)(clamp01(a) * 255.0f) << 24);
}

/* noir au debut (fondu entrant) et a la fin (fondu sortant), commun aux deux ecrans */
static float splash_dark(void) {
    float t = (float)s_splash_t;
    return clamp01((1.0f - clamp01(t / 18.0f)) + clamp01((t - (float)(SPLASH_FRAMES - 14)) / 14.0f));
}

static void splash_word(const char* word, float sc, float y, float t0, float par, float t) {
    float total = render_text_width(sc, word);
    float x0 = (TOP_W - total) * 0.5f + par;
    char prefix[16], one[2] = { 0, 0 };
    for (int i = 0; word[i] && i < 15; i++) {
        float a = clamp01((t - t0 - (float)i * 3.0f) / 20.0f);
        if (a <= 0.0f) continue;
        memcpy(prefix, word, (size_t)i);
        prefix[i] = 0;
        float lx = x0 + render_text_width(sc, prefix);
        float ly = y - (1.0f - ease_out_back(a)) * 70.0f;   /* la lettre tombe puis rebondit */
        one[0] = word[i];
        render_draw_text_depth(lx + 2.0f, ly + 2.0f, sc, with_alpha(T.black, a * 0.8f), 0.45f, one);
        render_draw_text(lx, ly, sc, with_alpha(T.text, a), one);
    }
}

static void draw_splash_top(float d3d) {
    const float t = (float)s_splash_t;
    const float ly = 158.0f, lh = 44.0f, lcy = ly + lh * 0.5f;
    ui_checker(TOP_W, TOP_H, (uint32_t)(s_splash_t * 3));

    /* titre */
    float k0 = ease_out_cubic(clamp01((t - 6.0f) / 24.0f));
    ui_text_c(TOP_W * 0.5f + d3d * 1.5f - (1.0f - k0) * 90.0f, 20.0f, 0.46f, with_alpha(T.accent, k0), "THE WORLD'S");
    splash_word("HARDEST", 1.3f, 38.0f, 16.0f, d3d * 2.5f, t);
    splash_word("GAMES", 1.3f, 76.0f, 38.0f, d3d * 2.5f, t);
    float k1 = clamp01((t - 70.0f) / 20.0f);
    ui_text_c(TOP_W * 0.5f + d3d * 1.5f, 124.0f, 0.46f, with_alpha(T.dim, k1), "4 jeux  -  1 hub");

    /* couloir qui s'ouvre depuis le centre */
    float lk = ease_out_cubic(clamp01((t - 40.0f) / 30.0f));
    float lw = (float)TOP_W * lk, lx0 = ((float)TOP_W - lw) * 0.5f;
    ui_rect(lx0, ly, lw, lh, T.surf);
    ui_rect(lx0, ly, lw, 1, T.line);
    ui_rect(lx0, ly + lh - 1, lw, 1, T.line);
    if (lk > 0.98f) {
        ui_rect(0, ly + 1, 64, lh - 2, T.goodZone);
        ui_rect(TOP_W - 64, ly + 1, 64, lh - 2, T.goodZone);
    }

    /* ennemis qui oscillent (en avant-plan en 3D) */
    for (int i = 0; i < 5; i++) {
        float g = clamp01((t - 70.0f - (float)i * 4.0f) / 12.0f);
        if (g <= 0.0f) continue;
        float cx = 100.0f + (float)i * 50.0f + d3d * 4.0f;
        float cy = lcy + sinf(t * 0.09f + (float)i * 1.3f) * 14.0f;
        ui_dot(cx, cy, 8.0f * g, T.black);
        ui_dot(cx, cy, 6.5f * g, T.blue);
    }

    /* le cube traverse le couloir en slalomant, avec une traine */
    float p = clamp01((t - 80.0f) / 130.0f);
    if (t >= 80.0f && p < 1.0f) {
        for (int gh = 4; gh >= 0; gh--) {
            float pp = p - (float)gh * 0.014f;
            if (pp < 0.0f) continue;
            float gx = -16.0f + pp * (float)(TOP_W + 8) + d3d * 6.0f;
            float gy = lcy + sinf(pp * 15.7f) * 11.0f;
            if (gh > 0) {
                ui_rect(gx - 8.0f, gy - 8.0f, 16.0f, 16.0f, with_alpha(render_skin_color(s_save.player_skin), 0.40f / (float)gh));
            } else {
                ui_rect(gx - 10.0f, gy - 10.0f, 20.0f, 20.0f, T.black);
                ui_rect(gx - 8.0f, gy - 8.0f, 16.0f, 16.0f, render_skin_color(s_save.player_skin));
            }
        }
    }

    /* arrivee : confettis + eclair */
    if (t >= 210.0f) {
        float dt = t - 210.0f;
        const u32 cols[4] = { T.accentHi, T.good, T.blue, C2D_Color32(0xF5, 0xB9, 0x2E, 0xFF) };
        if (dt < 70.0f) {
            for (int i = 0; i < 18; i++) {
                float ang = -3.14159f * (0.12f + 0.76f * (float)i / 17.0f);
                float sp = 1.6f + (float)(i % 5) * 0.55f;
                float cx = (float)TOP_W - 24.0f + cosf(ang) * sp * dt + d3d * 6.0f;
                float cy = lcy + sinf(ang) * sp * dt + 0.045f * dt * dt;
                ui_rect(cx - 2.0f, cy - 2.0f, 4.0f, 4.0f, with_alpha(cols[i & 3], 1.0f - dt / 70.0f));
            }
        }
        if (dt < 10.0f)
            C2D_DrawRectSolid(0, 0, 0.98f, TOP_W, TOP_H, C2D_Color32(255, 255, 255, (u8)((1.0f - dt / 10.0f) * 150.0f)));
    }

    if (t > 60.0f) {
        if (!audio_is_dsp_available()) {
            ui_text(28, 222, 0.34f, T.accentHi,
                    language_text(ui_language(), "Son indisponible : dspfirm.cdc manquant"));
        } else {
            ui_text_c(TOP_W * 0.5f, 222, 0.34f, T.muted,
                      language_text(ui_language(), "4 jeux  -  1 hub"));
        }
    }

    float dark = splash_dark();
    if (dark > 0.0f) C2D_DrawRectSolid(0, 0, 0.99f, TOP_W, TOP_H, C2D_Color32(0, 0, 0, (u8)(dark * 255.0f)));
}

static void draw_splash_bottom(void) {
    const float t = (float)s_splash_t;
    ui_rect(0, 0, BOT_W, BOT_H, T.bg);

    float k = ease_out_back(clamp01((t - 10.0f) / 30.0f));
    float sz = 30.0f * k * (1.0f + 0.06f * sinf(t * 0.12f));
    ui_rect(BOT_W * 0.5f - sz * 0.5f - 3.0f, 84.0f - sz * 0.5f - 3.0f, sz + 6.0f, sz + 6.0f, T.black);
    ui_rect(BOT_W * 0.5f - sz * 0.5f, 84.0f - sz * 0.5f, sz, sz, render_skin_color(s_save.player_skin));

    float a = clamp01((t - 40.0f) / 20.0f);
    ui_text_c(BOT_W * 0.5f, 120.0f, 0.62f, with_alpha(T.text, a), "WHG HUB");
    ui_text_c(BOT_W * 0.5f, 142.0f, 0.36f, with_alpha(T.muted, a), "v1.2  -  4 jeux reunis");

    /* L'intro sert aussi de mini-guide : les commandes sont visibles avant le menu. */
    float tips = ease_out_cubic(clamp01((t - 72.0f) / 22.0f));
    if (tips > 0.0f) {
        float y = 161.0f;
        ui_text_c(BOT_W * 0.5f, y,       0.36f, with_alpha(T.dim, tips), language_text(ui_language(), "D-Pad + stick : bouger"));
        ui_text_c(BOT_W * 0.5f, y + 12,  0.36f, with_alpha(T.dim, tips), language_text(ui_language(), "A : choisir   B : retour"));
        ui_text_c(BOT_W * 0.5f, y + 24,  0.36f, with_alpha(T.dim, tips), language_text(ui_language(), "SELECT : options"));
    }
    if (t > 120.0f) {
        float blink = 0.62f + 0.38f * sinf(t * 0.12f);
        ui_text_c(BOT_W * 0.5f, 202.0f, 0.36f, with_alpha(T.accentHi, blink),
                  language_text(ui_language(), "A / tactile : passer"));
    }
    float prog = clamp01(t / (float)SPLASH_FRAMES);
    ui_rect(100, 226, 120, 3, T.track);
    ui_rect(100, 226, (float)(int)(120.0f * prog), 3, T.accent);

    float dark = splash_dark();
    if (dark > 0.0f) C2D_DrawRectSolid(0, 0, 0.99f, BOT_W, BOT_H, C2D_Color32(0, 0, 0, (u8)(dark * 255.0f)));
}

static void draw_top(ScreenState ctx, float d3d) {
    const GameDef* cg = &g_games[s_active_game];
    char left[48], right[48];

    if (ctx == SCR_SPLASH) {
        draw_splash_top(d3d);
    } else if (ctx == SCR_HUB) {
        ui_top_menu(&g_games[s_hub_cursor], s_hub_cursor);
    } else if (ctx == SCR_TITLE) {
        ui_top_menu(cg, s_active_game);
    } else if (ctx == SCR_SELECT || ctx == SCR_TUTORIAL) {
        u32 rc = T.dim;
        render_level_top(&s_preview_level, NULL, s_save.player_skin, d3d, 1.0f, true);
        if (ctx == SCR_SELECT) {
            const char* noun = language_text(ui_language(),
                (s_active_game == GAME_WHG3 || s_active_game == GAME_WHG4) ? "Monde" : "Niveau");
            snprintf(left, sizeof(left), language_text(ui_language(), "%s %d / %u"),
                     noun, s_select_id, (unsigned)cg->main_levels);
            int bd = s_save.best_deaths[s_active_game][s_select_id - 1];
            if (level_locked(s_select_id)) {
                snprintf(right, sizeof(right), "%s", language_text(ui_language(), "Verrouille"));
                rc = T.accentHi;
            } else if (bd >= 0) {
                uint32_t bt = s_save.best_ticks[s_active_game][s_select_id - 1];
                if (bt > 0) {
                    char tb[16];
                    fmt_time(tb, sizeof(tb), bt);
                    snprintf(right, sizeof(right), language_text(ui_language(), "Record : %d morts  -  %s"), bd, tb);
                } else {
                    snprintf(right, sizeof(right), language_text(ui_language(), "Record : %d morts"), bd);
                }
                rc = T.good;
            } else if (s_active_game == GAME_WHG4) {
                /* avant : le nombre de pieces de la salle de depart, affiche comme total du monde */
                snprintf(right, sizeof(right), "$%u", (unsigned)WHG4_LEVEL_CASH[s_select_id - 1]);
            } else {
                uint16_t nc = (s_active_game == GAME_WHG3) ? WHG3_LEVEL_COINS[s_select_id - 1] : s_preview_level.num_coins;
                snprintf(right, sizeof(right), language_text(ui_language(), "%u pieces"), (unsigned)nc);
            }
        } else {
            snprintf(left, sizeof(left), language_text(ui_language(), "Tutoriel %d / %u"),
                     s_tut_cursor + 1, (unsigned)cg->tutorial_levels);
            snprintf(right, sizeof(right), language_text(ui_language(), "%u pieces"),
                     (unsigned)s_preview_level.num_coins);
        }
        ui_top_bar(left, right, rc);
    } else if (ctx == SCR_ACHIEVEMENTS) {
        render_level_top(&s_preview_level, NULL, s_save.player_skin, d3d, 1.0f, true);
        C2D_DrawRectSolid(0.0f, 0.0f, 0.8f, TOP_W, TOP_H, C2D_Color32(0x05, 0x06, 0x09, 0x9A));
        const AchievementDef* def = achievement_get(s_active_game, s_achievement_cursor);
        if (def) {
            /* Les textes sont a la profondeur 0.9 : a 0.5 ils etaient caches par le panneau (0.82) */
            bool done = achievement_is_unlocked(&s_save, s_active_game, s_achievement_cursor);
            uint32_t progress = achievement_progress(&s_save, s_active_game, s_achievement_cursor);
            if (progress > def->target) progress = def->target;
            uint8_t pct = achievement_percent(&s_save, s_active_game, s_achievement_cursor);
            u32 accent = done ? T.good : T.accent;
            char stat[48];
            const float PX = 14.0f, PY = 140.0f, PW = TOP_W - 28.0f, PH = 88.0f;
            C2D_DrawRectSolid(PX, PY, 0.82f, PW, PH, C2D_Color32(0x12, 0x14, 0x1A, 0xEE));
            C2D_DrawRectSolid(PX, PY, 0.83f, 3.0f, PH, accent);
            render_draw_text_depth(PX + 16.0f, PY + 8.0f, 0.6f, T.text, 0.9f,
                                   language_text(ui_language(), def->name));
            render_draw_text_depth(PX + 16.0f, PY + 33.0f, 0.44f, T.dim, 0.9f,
                                   language_text(ui_language(), def->description));
            float bx = PX + 16.0f, bw = PW - 32.0f;
            C2D_DrawRectSolid(bx, PY + 60.0f, 0.83f, bw, 4.0f, T.track);
            if (pct > 0) C2D_DrawRectSolid(bx, PY + 60.0f, 0.84f, (float)(int)(bw * (float)pct / 100.0f), 4.0f, accent);
            snprintf(stat, sizeof(stat), "%lu / %lu  -  %u%%",
                     (unsigned long)progress, (unsigned long)def->target, (unsigned)pct);
            render_draw_text_depth(bx, PY + 67.0f, 0.4f, done ? T.good : T.accentHi, 0.9f, stat);
            if (done) {
                const char* tag = language_text(ui_language(), "OBTENU");
                render_draw_text_depth(bx + bw - render_text_width(0.4f, tag), PY + 67.0f, 0.4f, T.good, 0.9f, tag);
            }
        }
    } else if (ctx == SCR_VICTORY) {
        const float X = 28.0f;
        char b[64];
        ui_checker(TOP_W, TOP_H, s_frame);
        ui_text(X, 34, 0.44f, T.good, "TERMINE");
        ui_text(X, 50, 0.92f, T.text, "Felicitations");
        snprintf(b, sizeof(b), language_text(ui_language(), "Vous avez termine %s."), cg->short_name);
        ui_text(X, 86, 0.5f, T.dim, b);
        ui_text(X, 114, 0.36f, T.muted, "MORTS TOTALES");
        snprintf(b, sizeof(b), "%lu", (unsigned long)s_save.total_deaths[s_active_game]);
        ui_text(X, 126, 0.78f, T.text, b);
        ui_lane(168, s_frame, TOP_W - 48);
        ui_status_line();
    } else if (ctx == SCR_LANGUAGE) {
        ui_checker(TOP_W, TOP_H, s_frame);
        ui_text(28.0f, 52.0f, 0.9f, T.text, "Choisir la langue");
        ui_text(28.0f, 94.0f, 0.62f, T.accent, language_name(s_language_choice));
    } else if (ctx == SCR_RESET_CONFIRM) {
        ui_checker(TOP_W, TOP_H, s_frame);
        ui_text(28.0f, 52.0f, 0.72f, T.accentHi, "Tout reinitialiser");
        ui_text(28.0f, 92.0f, 0.5f, T.text, "Niveaux, records et succes seront effaces.");
    } else if (s_level_loaded) {
        render_level_top(&s_level, &s_game, s_save.player_skin, d3d, get_zoom_factor(), false);
        if (ctx == SCR_PAUSE) {
            C2D_DrawRectSolid(0, 0, 0.8f, TOP_W, TOP_H, T.scrimDark);
            const char* pt = language_text(ui_language(), "Pause");
            render_draw_text_centered_depth(TOP_W * 0.5f + 1.5f, 99.5f, 0.9f, C2D_Color32(0, 0, 0, 190), 0.89f, pt);
            render_draw_text_centered_depth(TOP_W * 0.5f, 98.0f, 0.9f, T.text, 0.9f, pt);
        } else if (ctx == SCR_PLAYING) {
            draw_level_banner();
        }
    }
    draw_achievement_toast();
    draw_fade(TOP_W, TOP_H);
}

/* ---------- ecrans du bas ---------- */

static void draw_hud(const GameDef* cg) {
    if (!s_save.show_hud) {
        ui_text_c(BOT_W * 0.5f, 98.0f, 0.56f, T.dim, "HUD masque");
        ui_text_c(BOT_W * 0.5f, 120.0f, 0.4f, T.muted, "Appuyez sur X pour afficher");
        return;
    }
    char buf[64];
    uint16_t tot_c = s_game.is_whg4 ? WHG4_LEVEL_CASH[s_game.cur_level - 1]
                   : (s_game.is_whg3 ? WHG3_LEVEL_COINS[s_game.cur_level - 1] : s_level.num_coins);
    uint16_t cur_c = s_game.is_whg4 ? game_total_cash(&s_game) : s_game.coins_collected_count;

    if (s_game.is_whg3 || s_game.is_whg4) {
        snprintf(buf, sizeof(buf), language_text(ui_language(), "Monde %u / 5  -  Salle %c%u"),
                 s_game.cur_level, s_game.cur_col, s_game.cur_row);
    } else if (s_level.id <= cg->main_levels) {
        snprintf(buf, sizeof(buf), language_text(ui_language(), "Niveau %u / %u"),
                 s_level.id, cg->main_levels);
    } else {
        snprintf(buf, sizeof(buf), language_text(ui_language(), "Tutoriel %u / %u"),
                 s_level.id - cg->main_levels, cg->tutorial_levels);
    }
    ui_text(14, 9, 0.5f, T.text, buf);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)s_game.deaths);
    ui_stat_tile(12, 32, 96, 50, "MORTS", buf, T.accentHi, -1.0f, 0);

    if (s_game.is_whg4) snprintf(buf, sizeof(buf), "$%u / $%u", cur_c, tot_c);
    else                snprintf(buf, sizeof(buf), "%u / %u", cur_c, tot_c);
    ui_stat_tile(112, 32, 96, 50, s_game.is_whg4 ? "ARGENT" : "PIECES", buf, T.text,
                 (tot_c > 0) ? (float)cur_c / (float)tot_c : 1.0f, T.good);

    if (s_save.show_timer) {
        u32 ds = (s_game.level_ticks * 10) / 60;
        snprintf(buf, sizeof(buf), "%lu:%02lu.%lu",
                 (unsigned long)(ds / 600), (unsigned long)((ds / 10) % 60), (unsigned long)(ds % 10));
        ui_stat_tile(212, 32, 96, 50, "TEMPS", buf, T.text, -1.0f, 0);
    } else {
        const char* z = (s_save.zoom_mode == 1) ? "x1.5" : (s_save.zoom_mode == 2) ? "x2.0" : "x1.0";
        ui_stat_tile(212, 32, 96, 50, "ZOOM", z, T.text, -1.0f, 0);
    }

    ui_rrect(12, 90, 296, 104, T.surf);
    render_minimap(&s_level, &s_game, 20.0f, 96.0f, 132.0f, 92.0f);
    ui_rect(160, 98, 1, 88, T.line);
    static const UiHint hints[4] = {
        { "L/R", "Zoom" }, { "B", "Recommencer" }, { "X", "Masquer HUD" }, { "START", "Pause" }
    };
    for (int i = 0; i < 4; i++) {
        float ry = 99.0f + (float)i * 22.0f;
        ui_keycap(172.0f, ry, hints[i].key);
        ui_text(218.0f, ui_midy(ry, 16.0f, 0.4f), 0.4f, T.dim, hints[i].label);
    }

    static const char* const names[3] = { "Pause", "Recommencer", "Menu" };
    for (int i = 0; i < 3; i++) {
        UiButton b = lay_hud_btn(i);
        ui_rrect(b.x, b.y, b.w, b.h, T.surf2);
        ui_text_c(b.x + b.w * 0.5f, ui_midy(b.y, b.h, 0.46f), 0.46f, T.text, names[i]);
    }
}

static void draw_bottom(const GameDef* cg) {
    char buf[48];

    if (s_screen == SCR_SPLASH) {
        draw_splash_bottom();

    } else if (s_screen == SCR_HUB) {
        ui_header("Choisir un jeu", NULL, "Options");
        ui_text_c(204.0f, ui_midy(0, 32, 0.36f), 0.36f, T.dim, "v1.2");
        for (int i = 0; i < NUM_GAMES; i++) {
            UiButton cb = lay_hub_card(i);
            int d = s_hub_anim - i * 4;           /* les cartes montent l'une apres l'autre */
            if (d < 0) continue;
            if (d < 12) { float kk = 1.0f - (float)d / 12.0f; cb.y += (int)(kk * kk * 26.0f); }
            ui_game_card(cb, &g_games[i], i, i == s_hub_cursor);
        }
        static const UiHint h[2] = { { "A", "Ouvrir" }, { "SELECT", "Options" } };
        ui_footer(h, 2);

    } else if (s_screen == SCR_TITLE) {
        bool has_tut = (cg->tutorial_levels > 0);
        int n = has_tut ? 5 : 4;
        ui_header(cg->short_name, "Hub", NULL);
        char h0[24], h1[24], h2[24], h4[24];
        snprintf(h0, sizeof(h0), language_text(ui_language(), "Niveau %u"),
                 (unsigned)s_save.unlocked_level[s_active_game]);
        snprintf(h1, sizeof(h1), "1 - %u", (unsigned)cg->main_levels);
        snprintf(h2, sizeof(h2), language_text(ui_language(), "%u niveaux"),
                 (unsigned)cg->tutorial_levels);
        snprintf(h4, sizeof(h4), "%u%%", achievement_completion_percent(&s_save, s_active_game));
        const char* labels[5] = {
            s_save.unlocked_level[s_active_game] > 1 ? "Continuer" : "Jouer",
            "Niveaux",
            has_tut ? "Tutoriel" : "Options",
            has_tut ? "Options" : "Succes",
            "Succes"
        };
        const char* hints[5] = { h0, h1, has_tut ? h2 : "", has_tut ? "" : h4, h4 };
        for (int i = 0; i < n; i++)
            ui_menu_row(lay_menu_row(i, n), labels[i], hints[i], i == s_menu_cursor, i == 0);
        static const UiHint h[3] = { { "A", "Valider" }, { "B", "Hub" }, { "SELECT", "Options" } };
        ui_footer(h, 3);

    } else if (s_screen == SCR_SELECT) {
        if (s_active_game == GAME_WHG3 || s_active_game == GAME_WHG4) {
            scan_world_rooms(s_active_game, s_select_id);
            ui_header("Choisir une salle", "Retour", NULL);
            for (int w = 0; w < 5; w++) {
                UiButton t = lay_world_tab(w);
                bool sel = (w + 1 == s_select_id);
                bool beaten = (s_save.best_deaths[s_active_game][w] >= 0);
                ui_rrect(t.x, t.y, t.w, t.h, sel ? T.accent : T.surf2);
                snprintf(buf, sizeof(buf), language_text(ui_language(), "Monde %d"), w + 1);
                ui_text_c(t.x + t.w * 0.5f, ui_midy(t.y, t.h - 4.0f, 0.42f), 0.42f, sel ? T.text : T.dim, buf);
                if (beaten) ui_rect(t.x + (t.w - 14) / 2, t.y + t.h - 5, 14, 2, sel ? T.text : T.good);
            }
            for (int c = 0; c < 5; c++) {
                for (int r = 0; r < 5; r++) {
                    if (!s_room_exists[c][r]) continue;
                    UiButton rb = lay_room(c, r);
                    bool rsel = (s_sel_col == ('A' + c) && s_sel_row == (r + 1));
                    ui_rrect(rb.x, rb.y, rb.w, rb.h, rsel ? T.accent : T.surf2);
                    snprintf(buf, sizeof(buf), "%c%d", 'A' + c, r + 1);
                    ui_text_c(rb.x + rb.w * 0.5f, ui_midy(rb.y, rb.h, 0.44f), 0.44f, rsel ? T.text : T.dim, buf);
                }
            }
            static const UiHint h[3] = { { "A", "Jouer" }, { "B", "Retour" }, { "L/R", "Monde" } };
            ui_footer(h, 3);
            UiButton pb = lay_play();
            ui_rrect(pb.x, pb.y, pb.w, pb.h, T.accent);
            ui_text_c(pb.x + pb.w * 0.5f, ui_midy(pb.y, pb.h, 0.5f), 0.5f, T.text, "Jouer");
        } else {
            int max_lvl = cg->main_levels;
            ui_header("Niveaux", "Retour", NULL);
            unsigned unlocked = (unsigned)s_save.unlocked_level[s_active_game];
            for (int i = 0; i < max_lvl; i++) {
                int id = i + 1;
                UiButton c = lay_grid_cell(i, max_lvl);
                bool sel = (id == s_select_id);
                if (sel && s_deny > 0) c.x += (s_deny & 2) ? 3 : -3;
                bool beaten = (s_save.best_deaths[s_active_game][i] >= 0);
                bool ahead = ((unsigned)id > unlocked);
                if (sel)                          ui_rrect(c.x, c.y, c.w, c.h, T.accent);
                else if ((unsigned)id == unlocked) ui_ring(c.x, c.y, c.w, c.h, 1.0f, T.accent, T.surf2);
                else                              ui_rrect(c.x, c.y, c.w, c.h, T.surf2);
                snprintf(buf, sizeof(buf), "%d", id);
                ui_text_c(c.x + c.w * 0.5f, ui_midy(c.y, c.h, 0.44f), 0.44f,
                          sel ? T.text : (ahead ? T.muted : T.text), buf);
                if (beaten) ui_dot(c.x + c.w - 6.0f, c.y + 6.0f, 2.2f, sel ? T.text : T.good);
            }
            static const UiHint h[2] = { { "A", "Jouer" }, { "B", "Retour" } };
            ui_footer(h, 2);
        }

    } else if (s_screen == SCR_ACHIEVEMENTS) {
        int done_count = 0;
        for (int i = 0; i < ACHIEVEMENTS_PER_GAME; i++)
            if (achievement_is_unlocked(&s_save, s_active_game, i)) done_count++;
        char completion[16];
        snprintf(completion, sizeof(completion), "%d / %d", done_count, ACHIEVEMENTS_PER_GAME);
        ui_header("Succes", "Jeu", completion);

        const u32 gold   = C2D_Color32(0xF5, 0xB9, 0x2E, 0xFF);
        const u32 goldHi = C2D_Color32(0xFF, 0xD9, 0x6B, 0xFF);
        const u32 ink    = C2D_Color32(0x5A, 0x3D, 0x0A, 0xFF);

        for (int row = 0; row < ACHIEVEMENT_VISIBLE_ROWS; row++) {
            int index = s_achievement_first + row;
            if (index >= ACHIEVEMENTS_PER_GAME) break;
            const AchievementDef* def = achievement_get(s_active_game, index);
            if (!def) continue;

            UiButton card = lay_achievement_row(row);
            bool unlocked = achievement_is_unlocked(&s_save, s_active_game, index);
            bool sel = (index == s_achievement_cursor);
            uint8_t percent = achievement_percent(&s_save, s_active_game, index);
            uint32_t progress = achievement_progress(&s_save, s_active_game, index);
            if (progress > def->target) progress = def->target;

            u32 fill = sel ? T.surfHi : (unlocked ? T.goodZone : T.surf2);
            if (sel) ui_ring(card.x, card.y, card.w, card.h, 1.0f, unlocked ? T.good : T.accent, fill);
            else     ui_rrect(card.x, card.y, card.w, card.h, fill);

            /* medaille : doree + coche si obtenu, cadenas sinon */
            float bx = card.x + 21.0f, by = card.y + (card.h - 5.0f) * 0.5f + 1.0f;
            ui_dot(bx, by, 10.0f, unlocked ? gold : T.track);
            ui_dot(bx, by, 7.5f, unlocked ? goldHi : T.surf);
            if (unlocked) {
                C2D_DrawLine(bx - 3.5f, by + 0.5f, ink, bx - 1.0f, by + 3.0f, ink, 2.0f, UZ);
                C2D_DrawLine(bx - 1.0f, by + 3.0f, ink, bx + 4.0f, by - 3.0f, ink, 2.0f, UZ);
            } else {
                ui_rect(bx - 3.5f, by - 1.0f, 7.0f, 5.0f, T.muted);
                ui_rect(bx - 2.5f, by - 4.0f, 1.0f, 3.0f, T.muted);
                ui_rect(bx + 1.5f, by - 4.0f, 1.0f, 3.0f, T.muted);
                ui_rect(bx - 2.5f, by - 4.0f, 5.0f, 1.0f, T.muted);
            }

            float tx = card.x + 40.0f;
            ui_text(tx, card.y + 1.0f, 0.46f, T.text, def->name);

            char status[16];
            const char* st = status;
            if (unlocked) st = language_text(ui_language(), "OBTENU");
            else snprintf(status, sizeof(status), "%u%%", (unsigned)percent);
            float sw = render_text_width(0.38f, st);
            render_draw_text(card.x + card.w - 10.0f - sw, card.y + 3.0f, 0.38f, unlocked ? T.good : T.dim, st);

            ui_text(tx, card.y + 17.0f, 0.36f, T.dim, def->description);
            if (!unlocked) {
                char prog[24];
                snprintf(prog, sizeof(prog), "%lu / %lu", (unsigned long)progress, (unsigned long)def->target);
                float pw = render_text_width(0.34f, prog);
                render_draw_text(card.x + card.w - 10.0f - pw, card.y + 18.0f, 0.34f, T.muted, prog);
            }

            float barw = card.w - 40.0f - 10.0f;
            ui_rect(tx, card.y + card.h - 5.0f, barw, 2.0f, T.track);
            if (percent > 0)
                ui_rect(tx, card.y + card.h - 5.0f, (float)(int)(barw * (float)percent / 100.0f), 2.0f,
                        unlocked ? T.good : T.accent);
        }

        /* barre de defilement */
        {
            const float ty0 = 40.0f, th0 = 164.0f;
            float thumb = th0 * (float)ACHIEVEMENT_VISIBLE_ROWS / (float)ACHIEVEMENTS_PER_GAME;
            float tpos = (th0 - thumb) * (float)s_achievement_first /
                         (float)(ACHIEVEMENTS_PER_GAME - ACHIEVEMENT_VISIBLE_ROWS);
            ui_rect(315.0f, ty0, 3.0f, th0, T.surf2);
            ui_rect(315.0f, ty0 + tpos, 3.0f, thumb, T.accent);
        }
        static const UiHint h[2] = { { "D-PAD", "Defiler" }, { "B", "Retour" } };
        ui_footer(h, 2);

    } else if (s_screen == SCR_TUTORIAL) {
        static const char* const labels[4] = { "1.  Bases", "2.  Ennemis", "3.  Cles", "4.  Pieces" };
        int n = cg->tutorial_levels;
        if (n > 4) n = 4;
        ui_header("Tutoriel", "Retour", NULL);
        for (int i = 0; i < n; i++)
            ui_menu_row(lay_menu_row(i, n), labels[i], "", i == s_tut_cursor, false);
        static const UiHint h[2] = { { "A", "Jouer" }, { "B", "Retour" } };
        ui_footer(h, 2);

    } else if (s_screen == SCR_PLAYING && s_level_loaded) {
        draw_hud(cg);

    } else if (s_screen == SCR_PAUSE) {
        static const char* const labels[4] = { "Reprendre", "Recommencer", "Options", "Quitter au menu" };
        ui_header("Pause", NULL, NULL);
        for (int i = 0; i < 4; i++)
            ui_menu_row(lay_menu_row(i, 4), labels[i], "", i == s_menu_cursor, i == 0);
        static const UiHint h[2] = { { "A", "Valider" }, { "START", "Reprendre" } };
        ui_footer(h, 2);

    } else if (s_screen == SCR_OPTIONS) {
        static const char* const names[OPT_COUNT] = {
            "Son", "Couleur", "Chronometre", "Vitesse", "Zoom camera", "Langue",
            "Effets", "Mode sans mort", "Tout reinitialiser"
        };
        int option_count = options_count();
        ui_header("Options", "Retour", NULL);
        for (int i = 0; i < option_count; i++) {
            UiButton b = lay_opt_row(i);
            bool sel = (i == s_menu_cursor);
            float rx = b.x + b.w - 16.0f;
            float cy = b.y + b.h * 0.5f;
            ui_rrect(b.x, b.y, b.w, b.h, sel ? T.surfHi : T.surf);
            if (sel) ui_rect(b.x, b.y + 4, 3, b.h - 8, T.accent);
            ui_text(b.x + 16, ui_midy(b.y, b.h, 0.42f), 0.42f,
                    i == OPT_RESET ? T.accentHi : T.text, names[i]);
            switch (i) {
            case OPT_SOUND:     ui_toggle(rx - 30.0f, cy - 7.0f, !s_save.muted); break;
            case OPT_TIMER:     ui_toggle(rx - 30.0f, cy - 7.0f, s_save.show_timer != 0); break;
            case OPT_FX:        ui_toggle(rx - 30.0f, cy - 7.0f, s_save.fx_enabled != 0); break;
            case OPT_DISCOVERY: ui_toggle(rx - 30.0f, cy - 7.0f, s_save.discovery_mode != 0); break;
            case OPT_COLOR:
                snprintf(buf, sizeof(buf), "%d / 6", s_save.player_skin + 1);
                ui_text(b.x + 126, ui_midy(b.y, b.h, 0.42f), 0.42f, T.dim, buf);
                for (int k = 0; k < 6; k++) {   /* vraies couleurs du joueur */
                    float sx = rx - 9.0f - (float)(5 - k) * 12.0f;
                    if (k == s_save.player_skin) ui_rect(sx - 1.5f, cy - 6.0f, 12.0f, 12.0f, T.text);
                    ui_rect(sx, cy - 4.5f, 9.0f, 9.0f, render_skin_color(k));
                }
                break;
            case OPT_SPEED:
                ui_text(b.x + 126, ui_midy(b.y, b.h, 0.42f), 0.42f, T.dim, get_speed_label());
                ui_dots(rx, cy, 3, s_save.speed_mode, true);
                break;
            case OPT_ZOOM:
                ui_text(b.x + 126, ui_midy(b.y, b.h, 0.42f), 0.42f, T.dim, get_zoom_label());
                ui_dots(rx, cy, 3, s_save.zoom_mode, true);
                break;
            case OPT_LANG:
                ui_text(b.x + 126, ui_midy(b.y, b.h, 0.42f), 0.42f, T.dim, language_name(s_save.language));
                ui_dots(rx, cy, LANG_COUNT, s_save.language, false);
                break;
            default: break;
            }
        }
        static const UiHint h[2] = { { "A", "Modifier" }, { "B", "Retour" } };
        ui_footer(h, 2);

    } else if (s_screen == SCR_RESET_CONFIRM) {
        ui_header("Tout reinitialiser", "Retour", NULL);
        ui_text_c(BOT_W * 0.5f, 48.0f, 0.46f, T.text,
                  "Reinitialiser la progression des 4 jeux ?");
        ui_text_c(BOT_W * 0.5f, 67.0f, 0.38f, T.dim,
                  "Niveaux, records et succes seront effaces.");
        ui_text_c(BOT_W * 0.5f, 84.0f, 0.38f, T.dim,
                  "Les options et la langue seront conservees.");
        if (s_reset_save_failed)
            ui_text_c(BOT_W * 0.5f, 106.0f, 0.4f, T.accentHi,
                      "Echec de sauvegarde. Reessayez.");
        UiButton erase = { 16, 142, 288, 26, NULL };
        UiButton cancel = { 16, 174, 288, 26, NULL };
        ui_menu_row(erase, "Reinitialiser", "", s_menu_cursor == 0, true);
        ui_menu_row(cancel, "Annuler", "", s_menu_cursor == 1, false);
        static const UiHint h[2] = { { "A", "Valider" }, { "B", "Retour" } };
        ui_footer(h, 2);

    } else if (s_screen == SCR_LANGUAGE) {
        ui_header("Choisir la langue", NULL, NULL);
        for (int i = 0; i < LANG_COUNT; i++) {
            UiButton row = lay_menu_row(i, LANG_COUNT + 1);
            ui_menu_row(row, language_name((uint8_t)i), "", i == s_language_cursor, false);
        }
        UiButton ok = lay_menu_row(LANG_COUNT, LANG_COUNT + 1);
        ui_menu_row(ok, "OK", "", s_language_cursor == LANG_COUNT, true);
        static const UiHint h[2] = { { "D-PAD", "Selectionner" }, { "A", "Valider" } };
        ui_footer(h, 2);

    } else if (s_screen == SCR_VICTORY) {
        ui_rrect(60, 96, 200, 44, T.accent);
        ui_text_c(BOT_W * 0.5f, ui_midy(96, 44, 0.6f), 0.6f, T.text, "Continuer");
        ui_text_c(BOT_W * 0.5f, 152.0f, 0.4f, T.muted, "Touchez l'ecran ou appuyez sur A");
    }
    draw_fade(BOT_W, BOT_H);
}

/* ---------- simulation : un pas = une image de jeu a 60 Hz ---------- */

static void handle_room_warp(void) {
    /* Meme salle (cas d'une mort) : inutile de relire le fichier depuis la carte SD,
     * c'etait une micro-saccade a chaque mort. */
    bool same_room = (s_game.warp_col == s_game.cur_col && s_game.warp_row == s_game.cur_row);
    if (!same_room) render_fx_clear();   /* les particules appartiennent a l'ancienne salle */

    if (s_game.is_whg3) {
        if (same_room || load_whg3_room(s_game.cur_level, s_game.warp_col, s_game.warp_row)) {
            game_enter_room_whg3(&s_game, &s_level, s_game.warp_col, s_game.warp_row, s_game.warp_check_idx);
            begin_fade(8);
        } else {
            /* salle introuvable : avant, le joueur restait bloque en "mort" et le compteur montait a l'infini */
            restart_current_level();
        }
    } else if (s_game.is_whg4) {
        if (same_room || load_whg4_room(s_game.cur_level, s_game.warp_col, s_game.warp_row)) {
            game_enter_room_whg4(&s_game, &s_level, s_game.warp_col, s_game.warp_row,
                                 s_game.warp_place_at_spawn, s_game.warp_check_idx);
            if (s_game.warp_place_at_spawn) begin_fade(8);
        } else if (s_game.warp_col < s_game.cur_col) {
            s_game.player_x = s_game.player_half_size;
        } else if (s_game.warp_col > s_game.cur_col) {
            s_game.player_x = s_level.width - s_game.player_half_size;
        } else if (s_game.warp_row < s_game.cur_row) {
            s_game.player_y = 40.0f + s_game.player_half_size;
        } else if (s_game.warp_row > s_game.cur_row) {
            s_game.player_y = 560.0f - s_game.player_half_size;
        }
    }
    render_camera_reset();
}

static void play_step(u32 kHeld) {
    if (!s_level_loaded) return;
    const GameDef* cur_game = &g_games[s_active_game];
    render_fx_update();

    uint16_t coins_before = s_game.coins_collected_count;
    uint16_t diamonds_before = s_game.diamonds_collected_count;
    uint32_t keys_before = count_collected_keys(&s_game);
    uint32_t ev = game_update(&s_game, read_movement_input(kHeld), get_speed_multiplier());
    if (s_game.coins_collected_count > coins_before) {
        add_achievement_progress(&s_save.achievement_coins[s_active_game],
                                 s_game.coins_collected_count - coins_before);
    }
    if (s_game.diamonds_collected_count > diamonds_before) {
        add_achievement_progress(&s_save.achievement_diamonds[s_active_game],
                                 s_game.diamonds_collected_count - diamonds_before);
    }
    uint32_t keys_after = count_collected_keys(&s_game);
    if (keys_after > keys_before) {
        add_achievement_progress(&s_save.achievement_keys[s_active_game], keys_after - keys_before);
    }
    if (ev & GAME_EVT_CHECKPOINT) {
        add_achievement_progress(&s_save.achievement_checkpoints[s_active_game], 1);
    }
    s_save.total_deaths[s_active_game] = s_game.deaths;
    if (refresh_game_achievements(s_active_game, true))
        save_write(&s_save);
    if (ev & GAME_EVT_DIE)        audio_play_sfx(SFX_DIE);
    if (ev & GAME_EVT_DIE_WATER)  audio_play_sfx(SFX_DIE_WATER);
    if (ev & GAME_EVT_DIE_PIT)    audio_play_sfx(SFX_DIE_PIT);
    if (ev & GAME_EVT_COIN)       audio_play_sfx(SFX_COIN);
    if (ev & GAME_EVT_DIAMOND)    audio_play_sfx(SFX_DIAMOND);
    if (ev & GAME_EVT_KEY)        audio_play_sfx(SFX_KEY);
    if (ev & GAME_EVT_CHECKPOINT) audio_play_sfx(SFX_CHECK);

    {   /* effets visuels : au point exact ou l'evenement a eu lieu */
        float fx = s_game.player_x, fy = s_game.player_y;
        if (ev & (GAME_EVT_DIE | GAME_EVT_DIE_WATER | GAME_EVT_DIE_PIT)) {
            u32 body = (ev & GAME_EVT_DIE_WATER) ? C2D_Color32(0x3C, 0xAA, 0xFF, 0xFF)
                                                 : render_skin_color(s_save.player_skin);
            render_fx_burst(fx, fy, body, 22, 3.2f, 3.0f);
            render_fx_burst(fx, fy, C2D_Color32(0x14, 0x14, 0x1E, 0xFF), 8, 2.0f, 2.5f);
            render_fx_shake(3.5f);
        }
        if (ev & GAME_EVT_COIN)       render_fx_burst(fx, fy, C2D_Color32(0xFF, 0xE0, 0x20, 0xFF), 5, 1.6f, 2.0f);
        if (ev & GAME_EVT_DIAMOND)    render_fx_burst(fx, fy, C2D_Color32(0x50, 0xEB, 0xFF, 0xFF), 8, 2.0f, 2.2f);
        if (ev & GAME_EVT_KEY)        render_fx_burst(fx, fy, C2D_Color32(0xFF, 0xA8, 0x20, 0xFF), 8, 2.0f, 2.2f);
        if (ev & GAME_EVT_CHECKPOINT) render_fx_burst(fx, fy, C2D_Color32(0x45, 0xC8, 0x55, 0xFF), 10, 2.2f, 2.4f);
    }

    if (ev & GAME_EVT_ROOM_WARP) handle_room_warp();

    if (ev & GAME_EVT_WIN) {
        uint16_t cur_id = (s_game.is_whg3 || s_game.is_whg4) ? s_game.cur_level : s_level.id;
        s_save.total_deaths[s_active_game] = s_game.deaths;
        int idx = cur_id - 1;
        if (idx >= 0 && idx < MAX_LEVELS_PER_GAME) {
            uint32_t ld = s_game.level_deaths > 32767 ? 32767 : s_game.level_deaths;
            if (s_save.best_deaths[s_active_game][idx] < 0 ||
                (int)ld < s_save.best_deaths[s_active_game][idx]) {
                s_save.best_deaths[s_active_game][idx] = (int16_t)ld;
            }
            if (!s_game.invincible) {   /* pas de record de temps en "Mode sans mort" */
                uint32_t tk = s_game.level_ticks > 0 ? s_game.level_ticks : 1;
                uint32_t* bt = &s_save.best_ticks[s_active_game][idx];
                if (*bt == 0 || tk < *bt) *bt = tk;
            }
        }
        refresh_game_achievements(s_active_game, true);
        if (cur_id < cur_game->main_levels) {
            if (s_save.unlocked_level[s_active_game] <= cur_id) {
                s_save.unlocked_level[s_active_game] = cur_id + 1;
            }
            save_write(&s_save);
            audio_play_sfx(SFX_WIN);
            if (start_level(cur_id + 1)) {
                /* transition : fondu + carte "Monde N" / "Niveau N", jeu fige pendant ce temps */
                int hold = (s_game.is_whg3 || s_game.is_whg4) ? 48 : 30;
                s_hold = hold;
                s_hold_max = hold;
            } else {
                s_screen = SCR_TITLE;   /* avant : bloque sur un niveau deja gagne si le fichier manquait */
            }
        } else if (cur_id == cur_game->main_levels) {
            save_write(&s_save);
            audio_play_sfx(SFX_WIN_GAME);
            s_screen = SCR_VICTORY;
        } else {
            save_write(&s_save);
            audio_play_sfx(SFX_WIN);
            if (cur_id < cur_game->total_levels) {
                if (!start_level(cur_id + 1)) s_screen = SCR_TITLE;
            } else {
                s_screen = SCR_TITLE;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    romfsInit();
    cfguInit();

    APT_CheckNew3DS(&s_is_n3ds);
    if (s_is_n3ds) {
        osSetSpeedupEnable(true); // 804 MHz + L2 Cache on New 3DS
    }

    gfxInitDefault();
    /* Etait gfxSet3D(false) : l'oeil droit etait dessine pour rien quand le curseur 3D etait leve
     * (jeu ralenti) et la profondeur n'apparaissait jamais. */
    gfxSet3D(true);
    /* plus de marge : les grandes salles WHG3/4 depassaient 4096 objets sur Old 3DS (sprites qui disparaissent) */
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 2);
    C2D_Init(8192);
    C2D_Prepare();

    C3D_RenderTarget* top_left  = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* top_right = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    C3D_RenderTarget* bot       = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    render_init();
    audio_init();
    bool save_exists = save_load(&s_save);
    if (!save_exists || s_save.language == LANGUAGE_UNSET) {
        s_after_splash = SCR_LANGUAGE;   /* premier lancement : choix de la langue apres l'intro */
        s_language_choice = LANG_FRENCH;
        s_language_cursor = LANG_FRENCH;
    }
    s_screen = SCR_SPLASH;
    s_splash_t = 0;
    bool achievements_updated = false;
    for (int game = 0; game < NUM_GAMES; game++) {
        achievements_updated |= refresh_game_achievements(game, false);
    }
    if (achievements_updated) save_write(&s_save);
    audio_set_muted(s_save.muted != 0);
    render_fx_enable(s_save.fx_enabled != 0);

    ui_init();
    s_last_tick = svcGetSystemTick();

    while (aptMainLoop()) {
        hidScanInput();
        audio_update();

        /* temps reel ecoule : la simulation reste a vitesse constante meme si l'affichage rame */
        u64 now_tick = svcGetSystemTick();
        double frame_dt = (double)(now_tick - s_last_tick) / (double)SYSCLOCK_ARM11;
        s_last_tick = now_tick;
        if (frame_dt < 0.0 || frame_dt > 0.25) frame_dt = STEP_SECONDS;   /* veille, chargement long... */
        s_acc += frame_dt;

        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        bool navUp    = (kDown & (KEY_DUP | KEY_CPAD_UP)) != 0;
        bool navDownB = (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) != 0;
        bool navLeft  = (kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) != 0;
        bool navRight = (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) != 0;
        touchPosition touch;
        bool touched = (kDown & KEY_TOUCH) != 0;
        if (touched) hidTouchRead(&touch);

        if ((kDown & KEY_SELECT) && s_screen != SCR_OPTIONS && s_screen != SCR_LANGUAGE &&
            s_screen != SCR_RESET_CONFIRM && s_screen != SCR_SPLASH) {
            open_options(s_screen);
            /* BUG corrige : la meme pression de SELECT etait relue juste apres par l'ecran Options
             * (SELECT = fermer) qui se refermait dans la meme image : SELECT semblait ne rien faire. */
            kDown &= ~KEY_SELECT;
        }

        const GameDef* cur_game = &g_games[s_active_game];

        // --- UPDATE ---
        if (s_screen == SCR_SPLASH) {
            s_splash_t++;
            /* n'importe quelle touche passe l'intro (apres 0,5 s), en lancant le fondu de sortie */
            if (s_splash_t > 30 && s_splash_t < SPLASH_FRAMES - 14 &&
                (kDown & (KEY_A | KEY_B | KEY_START | KEY_TOUCH)))
                s_splash_t = SPLASH_FRAMES - 14;
            if (s_splash_t >= SPLASH_FRAMES) {
                s_screen = s_after_splash;
                s_hub_anim = 0;
                begin_fade(14);
            }
        } else if (s_screen == SCR_LANGUAGE) {
            if (navDownB) s_language_cursor = (s_language_cursor + 1) % (LANG_COUNT + 1);
            if (navUp) s_language_cursor =
                (s_language_cursor + LANG_COUNT) % (LANG_COUNT + 1);
            if (s_language_cursor < LANG_COUNT)
                s_language_choice = (uint8_t)s_language_cursor;

            bool confirm = false;
            if (touched) {
                for (int i = 0; i < LANG_COUNT; i++) {
                    UiButton row = lay_menu_row(i, LANG_COUNT + 1);
                    if (ui_button_hit(&row, touch.px, touch.py)) {
                        s_language_cursor = i;
                        s_language_choice = (uint8_t)i;
                        break;
                    }
                }
                UiButton ok = lay_menu_row(LANG_COUNT, LANG_COUNT + 1);
                if (ui_button_hit(&ok, touch.px, touch.py)) confirm = true;
            }
            if (kDown & KEY_A) {
                if (s_language_cursor < LANG_COUNT) s_language_cursor = LANG_COUNT;
                else confirm = true;
            }
            if (confirm) {
                s_save.language = s_language_choice;
                save_write(&s_save);
                s_screen = SCR_HUB;
                s_hub_anim = 0;
                begin_fade(14);
            }
        } else if (s_screen == SCR_HUB) {
            if (navRight || navLeft) s_hub_cursor ^= 1;
            if (navDownB || navUp)   s_hub_cursor ^= 2;

            int act = (kDown & KEY_A) ? s_hub_cursor : -1;
            if (touched) {
                UiButton opt_btn = lay_hub_options();
                if (ui_button_hit(&opt_btn, touch.px, touch.py)) {
                    open_options(SCR_HUB);
                } else {
                    for (int i = 0; i < NUM_GAMES; i++) {
                        UiButton card = lay_hub_card(i);
                        if (ui_button_hit(&card, touch.px, touch.py)) {
                            s_hub_cursor = i;
                            act = i;
                            break;
                        }
                    }
                }
            }
            if (act >= 0 && g_games[act].available) {
                enter_game(act);
            }
        } else if (s_screen == SCR_TITLE) {
            bool has_tut = (cur_game->tutorial_levels > 0);
            int num_btns = has_tut ? 5 : 4;

            if (navDownB) s_menu_cursor = (s_menu_cursor + 1) % num_btns;
            if (navUp)    s_menu_cursor = (s_menu_cursor + num_btns - 1) % num_btns;
            if (kDown & KEY_B) {
                return_to_hub();
            } else {
                int act = (kDown & KEY_A) ? s_menu_cursor : -1;
                if (touched) {
                    UiButton hub_btn = lay_back();
                    if (ui_button_hit(&hub_btn, touch.px, touch.py)) {
                        return_to_hub();
                    } else {
                        for (int i = 0; i < num_btns; i++) {
                            UiButton b = lay_menu_row(i, num_btns);
                            if (ui_button_hit(&b, touch.px, touch.py)) act = i;
                        }
                    }
                }
                if (s_screen == SCR_TITLE) {
                    if (act == 0) {
                        start_level(s_save.unlocked_level[s_active_game]);
                    } else if (act == 1) {
                        s_screen = SCR_SELECT;
                        load_preview(s_select_id);
                    } else if (has_tut && act == 2) {
                        s_screen = SCR_TUTORIAL;
                        s_tut_cursor = 0;
                        load_preview(cur_game->main_levels + 1);
                    } else if ((has_tut && act == 3) || (!has_tut && act == 2)) {
                        open_options(SCR_TITLE);
                    } else if ((has_tut && act == 4) || (!has_tut && act == 3)) {
                        s_screen = SCR_ACHIEVEMENTS;
                        s_achievement_cursor = 0;
                        s_achievement_first = 0;
                    }
                }
            }
        } else if (s_screen == SCR_SELECT) {
            if (s_active_game == GAME_WHG3 || s_active_game == GAME_WHG4) {
                scan_world_rooms(s_active_game, s_select_id);
                if (kDown & KEY_R) {
                    s_select_id = (s_select_id >= 5) ? 1 : s_select_id + 1;
                    scan_world_rooms(s_active_game, s_select_id);
                }
                if (kDown & KEY_L) {
                    s_select_id = (s_select_id <= 1) ? 5 : s_select_id - 1;
                    scan_world_rooms(s_active_game, s_select_id);
                }
                int ci = s_sel_col - 'A', ri = s_sel_row - 1;
                if (navRight) {
                    for (int dc = 1; dc < 5; dc++) {
                        int nc = (ci + dc) % 5;
                        if (s_room_exists[nc][ri]) { s_sel_col = 'A' + nc; break; }
                    }
                }
                if (navLeft) {
                    for (int dc = 1; dc < 5; dc++) {
                        int nc = (ci + 5 - dc) % 5;
                        if (s_room_exists[nc][ri]) { s_sel_col = 'A' + nc; break; }
                    }
                }
                if (navDownB) {
                    for (int dr = 1; dr < 5; dr++) {
                        int nr = (ri + dr) % 5;
                        if (s_room_exists[ci][nr]) { s_sel_row = nr + 1; break; }
                    }
                }
                if (navUp) {
                    for (int dr = 1; dr < 5; dr++) {
                        int nr = (ri + 5 - dr) % 5;
                        if (s_room_exists[ci][nr]) { s_sel_row = nr + 1; break; }
                    }
                }
                load_preview_room(s_select_id, s_sel_col, s_sel_row);

                if (kDown & KEY_B) s_screen = SCR_TITLE;
                if (kDown & KEY_A) try_start_room(s_select_id, s_sel_col, s_sel_row);
                if (touched) {
                    UiButton back_btn = lay_back();
                    UiButton play_btn = lay_play();
                    if (ui_button_hit(&back_btn, touch.px, touch.py)) {
                        s_screen = SCR_TITLE;
                    } else if (ui_button_hit(&play_btn, touch.px, touch.py)) {
                        try_start_room(s_select_id, s_sel_col, s_sel_row);
                    } else {
                        for (int w = 0; w < 5; w++) {
                            UiButton wb = lay_world_tab(w);
                            if (ui_button_hit(&wb, touch.px, touch.py)) {
                                s_select_id = w + 1;
                                scan_world_rooms(s_active_game, s_select_id);
                                load_preview_room(s_select_id, s_sel_col, s_sel_row);
                            }
                        }
                        for (int c = 0; c < 5; c++) {
                            for (int r = 0; r < 5; r++) {
                                if (!s_room_exists[c][r]) continue;
                                UiButton rb = lay_room(c, r);
                                if (ui_button_hit(&rb, touch.px, touch.py)) {
                                    s_sel_col = 'A' + c;
                                    s_sel_row = r + 1;
                                    load_preview_room(s_select_id, s_sel_col, s_sel_row);
                                }
                            }
                        }
                    }
                }
            } else {
                int max_lvl = cur_game->main_levels;
                int cols = (max_lvl <= 30) ? 6 : 10;
                if (navRight) { if (s_select_id < max_lvl) s_select_id++; }
                if (navLeft)  { if (s_select_id > 1) s_select_id--; }
                if (navDownB) { if (s_select_id + cols <= max_lvl) s_select_id += cols; }
                if (navUp)    { if (s_select_id - cols >= 1) s_select_id -= cols; }
                if (kDown & KEY_R) { if (s_select_id < max_lvl) s_select_id++; }
                if (kDown & KEY_L) { if (s_select_id > 1) s_select_id--; }
                load_preview(s_select_id);

                if (kDown & KEY_B) s_screen = SCR_TITLE;
                if (kDown & KEY_A) try_start_level(s_select_id);
                if (touched) {
                    UiButton back_btn = lay_back();
                    if (ui_button_hit(&back_btn, touch.px, touch.py)) {
                        s_screen = SCR_TITLE;
                    } else {
                        for (int i = 0; i < max_lvl; i++) {
                            UiButton cell = lay_grid_cell(i, max_lvl);
                            if (ui_button_hit(&cell, touch.px, touch.py)) {
                                s_select_id = i + 1;
                                load_preview(s_select_id);
                                try_start_level(s_select_id);
                                break;
                            }
                        }
                    }
                }
            }
        } else if (s_screen == SCR_ACHIEVEMENTS) {
            if (navDownB) s_achievement_cursor = (s_achievement_cursor + 1) % ACHIEVEMENTS_PER_GAME;
            if (navUp) s_achievement_cursor = (s_achievement_cursor + ACHIEVEMENTS_PER_GAME - 1) %
                                             ACHIEVEMENTS_PER_GAME;
            if (navRight) {
                s_achievement_cursor += ACHIEVEMENT_VISIBLE_ROWS;
                if (s_achievement_cursor >= ACHIEVEMENTS_PER_GAME) s_achievement_cursor = ACHIEVEMENTS_PER_GAME - 1;
            }
            if (navLeft) {
                s_achievement_cursor -= ACHIEVEMENT_VISIBLE_ROWS;
                if (s_achievement_cursor < 0) s_achievement_cursor = 0;
            }
            achievement_scroll_fix();
            if (kDown & KEY_B) {
                s_screen = SCR_TITLE;
            } else if (touched) {
                UiButton back_btn = lay_back();
                if (ui_button_hit(&back_btn, touch.px, touch.py)) {
                    s_screen = SCR_TITLE;
                } else {
                    for (int row = 0; row < ACHIEVEMENT_VISIBLE_ROWS; row++) {
                        UiButton card = lay_achievement_row(row);
                        int index = s_achievement_first + row;
                        if (index < ACHIEVEMENTS_PER_GAME && ui_button_hit(&card, touch.px, touch.py)) {
                            s_achievement_cursor = index;
                            break;
                        }
                    }
                }
            }

        } else if (s_screen == SCR_TUTORIAL) {
            int n_tut = cur_game->tutorial_levels;
            if (navDownB) s_tut_cursor = (s_tut_cursor + 1) % n_tut;
            if (navUp)    s_tut_cursor = (s_tut_cursor + n_tut - 1) % n_tut;
            load_preview(cur_game->main_levels + 1 + s_tut_cursor);
            if (kDown & KEY_B) s_screen = SCR_TITLE;
            if (kDown & KEY_A) start_level(cur_game->main_levels + 1 + s_tut_cursor);
            if (touched) {
                UiButton back_btn = lay_back();
                if (ui_button_hit(&back_btn, touch.px, touch.py)) {
                    s_screen = SCR_TITLE;
                } else {
                    for (int i = 0; i < n_tut; i++) {
                        UiButton cell = lay_menu_row(i, n_tut);
                        if (ui_button_hit(&cell, touch.px, touch.py)) {
                            start_level(cur_game->main_levels + 1 + i);
                            break;
                        }
                    }
                }
            }
        } else if (s_screen == SCR_PLAYING && s_level_loaded) {
            if (kDown & KEY_START) {
                audio_play_sfx(SFX_PAUSE);
                s_screen = SCR_PAUSE;
                s_menu_cursor = 0;
            } else if (kDown & KEY_R) {
                s_save.zoom_mode = (s_save.zoom_mode + 1) % 3;
            } else if (kDown & KEY_L) {
                s_save.zoom_mode = (s_save.zoom_mode + 2) % 3;
            } else if (kDown & KEY_B) {
                restart_current_level();
            } else if (kDown & KEY_X) {
                s_save.show_hud = !s_save.show_hud;
            } else if ((kDown & KEY_Y) && s_save.discovery_mode) {
                /* passer le niveau : reserve au "Mode sans mort" (avant : un appui accidentel sur Y suffisait) */
                uint16_t cur_id = (s_game.is_whg3 || s_game.is_whg4) ? s_game.cur_level : s_level.id;
                if (cur_id < cur_game->main_levels) {
                    if (s_save.unlocked_level[s_active_game] <= cur_id) {
                        s_save.unlocked_level[s_active_game] = cur_id + 1;
                        save_write(&s_save);
                    }
                    if (!start_level(cur_id + 1)) s_screen = SCR_TITLE;
                }
            } else {
                if (touched) {
                    UiButton b_pause   = lay_hud_btn(0);
                    UiButton b_restart = lay_hud_btn(1);
                    UiButton b_menu    = lay_hud_btn(2);
                    if (ui_button_hit(&b_pause, touch.px, touch.py)) {
                        audio_play_sfx(SFX_PAUSE);
                        s_screen = SCR_PAUSE;
                        s_menu_cursor = 0;
                    } else if (ui_button_hit(&b_restart, touch.px, touch.py)) {
                        restart_current_level();
                    } else if (ui_button_hit(&b_menu, touch.px, touch.py)) {
                        s_save.total_deaths[s_active_game] = s_game.deaths;
                        save_write(&s_save);
                        s_screen = SCR_TITLE;
                    }
                }

                if (s_screen == SCR_PLAYING) {
                    int steps = 0;
                    if (s_hold > 0) {
                        s_hold--;            /* carte de titre : le jeu attend */
                        s_acc = 0.0;
                    } else {
                        /* nombre de pas a jouer pour rattraper le temps reel (1 si 60 FPS, 2 si 30 FPS...) */
                        steps = (int)(s_acc / STEP_SECONDS + 0.5);
                        if (steps < 0) steps = 0;
                        if (steps > 4) steps = 4;
                        s_acc -= (double)steps * STEP_SECONDS;
                        if (s_acc > 2.0 * STEP_SECONDS || s_acc < -2.0 * STEP_SECONDS) s_acc = 0.0;
                    }
                    for (int i = 0; i < steps && s_screen == SCR_PLAYING && s_hold == 0; i++) {
                        play_step(kHeld);
                    }
                }
            }

        } else if (s_screen == SCR_PAUSE) {
            UiButton btns[4];
            for (int i = 0; i < 4; i++) btns[i] = lay_menu_row(i, 4);
            if (kDown & (KEY_START | KEY_B)) {
                s_screen = SCR_PLAYING;
            } else {
                if (navDownB) s_menu_cursor = (s_menu_cursor + 1) % 4;
                if (navUp)    s_menu_cursor = (s_menu_cursor + 3) % 4;
                int act = (kDown & KEY_A) ? s_menu_cursor : -1;
                if (touched) {
                    for (int i = 0; i < 4; i++) {
                        if (ui_button_hit(&btns[i], touch.px, touch.py)) act = i;
                    }
                }
                if (act == 0) {
                    s_screen = SCR_PLAYING;
                } else if (act == 1) {
                    restart_current_level();
                } else if (act == 2) {
                    open_options(SCR_PAUSE);
                } else if (act == 3) {
                    s_save.total_deaths[s_active_game] = s_game.deaths;
                    save_write(&s_save);
                    s_screen = SCR_TITLE;
                }
            }
        } else if (s_screen == SCR_RESET_CONFIRM) {
            if (navDownB || navUp) s_menu_cursor ^= 1;
            if (kDown & KEY_B) {
                s_screen = SCR_OPTIONS;
                s_menu_cursor = OPT_RESET;
            } else {
                int act = (kDown & KEY_A) ? s_menu_cursor : -1;
                if (touched) {
                    UiButton back_btn = lay_back();
                    UiButton erase = { 16, 142, 288, 26, NULL };
                    UiButton cancel = { 16, 174, 288, 26, NULL };
                    if (ui_button_hit(&back_btn, touch.px, touch.py) ||
                        ui_button_hit(&cancel, touch.px, touch.py)) {
                        act = 1;
                    } else if (ui_button_hit(&erase, touch.px, touch.py)) {
                        act = 0;
                    }
                }
                if (act == 0) {
                    if (reset_all_progress()) {
                        s_achievement_queue_count = 0;
                        s_achievement_toast_frames = 0;
                        s_hub_cursor = 0;
                        s_active_game = GAME_WHG1;
                        s_select_id = 1;
                        s_screen = SCR_HUB;
                        s_hub_anim = 0;
                    } else {
                        s_reset_save_failed = true;
                    }
                } else if (act == 1) {
                    s_screen = SCR_OPTIONS;
                    s_menu_cursor = OPT_RESET;
                }
            }
        } else if (s_screen == SCR_OPTIONS) {
            int option_count = options_count();
            if (navDownB) s_menu_cursor = (s_menu_cursor + 1) % option_count;
            if (navUp)    s_menu_cursor = (s_menu_cursor + option_count - 1) % option_count;
            if (kDown & (KEY_B | KEY_SELECT)) {
                close_options();
            } else {
                /* gauche / droite changent la valeur de la ligne (sauf "Tout reinitialiser") */
                int dir = navRight ? 1 : (navLeft ? -1 : 0);
                int act = (kDown & KEY_A) ? s_menu_cursor : -1;
                if (dir != 0 && s_menu_cursor != OPT_RESET) act = s_menu_cursor;
                if (touched) {
                    UiButton back_btn = lay_back();
                    if (ui_button_hit(&back_btn, touch.px, touch.py)) {
                        close_options();
                        act = -1;
                    } else {
                        for (int i = 0; i < option_count; i++) {
                            UiButton b = lay_opt_row(i);
                            if (ui_button_hit(&b, touch.px, touch.py)) { act = i; s_menu_cursor = i; }
                        }
                    }
                }
                int step = (dir != 0) ? dir : 1;
                switch (act) {
                case OPT_SOUND:
                    s_save.muted = !s_save.muted;
                    audio_set_muted(s_save.muted != 0);
                    break;
                case OPT_COLOR:
                    s_save.player_skin = (uint8_t)((s_save.player_skin + 6 + step) % 6);
                    break;
                case OPT_TIMER:
                    s_save.show_timer = !s_save.show_timer;
                    break;
                case OPT_SPEED:
                    s_save.speed_mode = (uint8_t)((s_save.speed_mode + 3 + step) % 3);
                    break;
                case OPT_ZOOM:
                    s_save.zoom_mode = (uint8_t)((s_save.zoom_mode + 3 + step) % 3);
                    break;
                case OPT_LANG:
                    s_save.language = (uint8_t)((s_save.language + LANG_COUNT + step) % LANG_COUNT);
                    break;
                case OPT_FX:
                    s_save.fx_enabled = !s_save.fx_enabled;
                    render_fx_enable(s_save.fx_enabled != 0);
                    break;
                case OPT_DISCOVERY:
                    s_save.discovery_mode = !s_save.discovery_mode;
                    if (s_level_loaded) s_game.invincible = s_save.discovery_mode != 0;
                    break;
                case OPT_RESET:
                    s_reset_save_failed = false;
                    s_screen = SCR_RESET_CONFIRM;
                    s_menu_cursor = 1;
                    break;
                default: break;
                }
            }
        } else if (s_screen == SCR_VICTORY) {
            if (kDown & (KEY_A | KEY_B | KEY_START | KEY_TOUCH)) {
                s_screen = SCR_TITLE;
                begin_fade(12);
            }
        }

        if (s_screen != SCR_PLAYING) s_acc = 0.0;

        // --- RENDER ---
        s_frame++;
        if (s_fade > 0) s_fade--;
        if (s_deny > 0) s_deny--;
        if (s_screen == SCR_HUB && s_hub_anim < 60) s_hub_anim++;
        update_achievement_toast();
        float slider = osGet3DSliderState();
        render_begin_frame();
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        // Les options s'affichent par-dessus l'ecran d'ou on vient
        ScreenState top_ctx = (s_screen == SCR_OPTIONS) ? s_options_return : s_screen;

        // camera : mise a jour une seule fois par image (et non une fois par oeil)
        if (s_level_loaded && (top_ctx == SCR_PLAYING || top_ctx == SCR_PAUSE))
            render_camera_update(&s_level, &s_game, get_zoom_factor());
        else
            render_camera_reset();

        int eyes = (slider > 0.0f) ? 2 : 1;
        for (int eye = 0; eye < eyes; eye++) {
            /* signe corrige : l'oeil gauche decale vers la droite -> murs et ennemis "sortent" de l'ecran
             * (avant : inverse, tout paraissait enfonce derriere le sol) */
            float d3d = (eye == 0) ? slider : -slider;
            C3D_RenderTarget* target = (eye == 0) ? top_left : top_right;
            C2D_TargetClear(target, T.bg);
            C2D_SceneBegin(target);
            draw_top(top_ctx, d3d);
        }

        C2D_TargetClear(bot, T.bg);
        C2D_SceneBegin(bot);
        draw_bottom(cur_game);

        C3D_FrameEnd(0);
        audio_update();
    }

    save_write(&s_save);
    level_free(&s_level);
    level_free(&s_preview_level);
    game_free(&s_game);
    audio_exit();
    render_exit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    cfguExit();
    romfsExit();
    return 0;
}
