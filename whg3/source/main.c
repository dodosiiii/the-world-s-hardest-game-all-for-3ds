#include "level.h"
#include "game.h"
#include "render.h"
#include "audio.h"
#include "save.h"

#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    SCR_TITLE = 0,
    SCR_SELECT,
    SCR_PLAYING,
    SCR_PAUSE,
    SCR_OPTIONS,
    SCR_VICTORY
} ScreenState;

static SaveData s_save;
static LevelData s_level;
static LevelData s_preview_level;
static int s_preview_id = -1;
static GameState s_game;
static bool s_level_loaded = false;
static bool s_is_n3ds = false;

static ScreenState s_screen = SCR_TITLE;
static ScreenState s_options_return = SCR_TITLE;
static int s_menu_cursor = 0;
static int s_select_id = 1; // 1..5

static float get_speed_multiplier(void) {
    if (s_save.speed_mode == 1) return 1.25f;
    if (s_save.speed_mode == 2) return 1.50f;
    return 1.00f;
}

static const char* get_speed_label(void) {
    if (s_save.speed_mode == 1) return "RAPIDE (1.25x)";
    if (s_save.speed_mode == 2) return "TURBO (1.50x)";
    return "NORMALE (1.0x)";
}

static void load_preview(int id) {
    if (id == s_preview_id) return;
    level_free(&s_preview_level);
    const Whg3StartFinish* st = &WHG3_STARTS[id - 1];
    char path[64];
    snprintf(path, sizeof(path), "romfs:/levels/l%d_%c%u.lvl", id, st->col, st->row);
    if (level_load(&s_preview_level, path)) {
        s_preview_id = id;
    } else {
        s_preview_id = -1;
    }
}

static bool load_room(int level_num, char col, uint8_t row) {
    level_free(&s_level);
    s_level_loaded = false;
    char path[64];
    snprintf(path, sizeof(path), "romfs:/levels/l%d_%c%u.lvl", level_num, col, row);
    if (!level_load(&s_level, path)) return false;
    s_level_loaded = true;
    return true;
}

static bool start_level(int id) {
    game_free(&s_game);
    const Whg3StartFinish* st = &WHG3_STARTS[id - 1];
    if (!load_room(id, st->col, st->row)) return false;
    game_init_level(&s_game, (uint8_t)id, &s_level, s_save.total_deaths);
    s_screen = SCR_PLAYING;
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

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    romfsInit();
    cfguInit();

    APT_CheckNew3DS(&s_is_n3ds);
    if (s_is_n3ds) {
        osSetSpeedupEnable(true);
    }

    gfxInitDefault();
    gfxSet3D(false);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * (s_is_n3ds ? 2 : 1));
    C2D_Init(s_is_n3ds ? 8192 : C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* top_left  = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* top_right = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    C3D_RenderTarget* bot       = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    render_init();
    audio_init();
    save_load(&s_save);
    audio_set_muted(s_save.muted != 0);
    s_select_id = s_save.unlocked_level;
    load_preview(s_select_id);

    u32 clrBgTop   = C2D_Color32(0x18, 0x1D, 0x36, 0xFF);
    u32 clrBgBot   = C2D_Color32(0x0B, 0x0E, 0x1D, 0xFF);
    u32 clrPanel   = C2D_Color32(0x1E, 0x24, 0x42, 0xFF);
    u32 clrBtnTop  = C2D_Color32(0x36, 0x40, 0x70, 0xFF);
    u32 clrBtnBot  = C2D_Color32(0x24, 0x2B, 0x4E, 0xFF);
    u32 clrSelTop  = C2D_Color32(0x52, 0x62, 0xA8, 0xFF);
    u32 clrSelBot  = C2D_Color32(0x38, 0x44, 0x7A, 0xFF);
    u32 clrBeatTop = C2D_Color32(0x26, 0x6A, 0x44, 0xFF);
    u32 clrBeatBot = C2D_Color32(0x18, 0x48, 0x2E, 0xFF);
    u32 clrLockTop = C2D_Color32(0x1C, 0x1F, 0x30, 0xFF);
    u32 clrLockBot = C2D_Color32(0x14, 0x16, 0x24, 0xFF);
    u32 clrBorder  = C2D_Color32(0x70, 0x80, 0xC8, 0xFF);
    u32 clrWhite   = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 clrYellow  = C2D_Color32(0xFF, 0xDC, 0x48, 0xFF);
    u32 clrGreen   = C2D_Color32(0x60, 0xE8, 0x78, 0xFF);
    u32 clrRed     = C2D_Color32(0xFF, 0x52, 0x52, 0xFF);
    u32 clrCyan    = C2D_Color32(0x58, 0xD0, 0xFF, 0xFF);
    u32 clrGray    = C2D_Color32(0x88, 0x90, 0xAA, 0xFF);

    while (aptMainLoop()) {
        hidScanInput();
        audio_update();

        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        bool navUp    = (kDown & (KEY_DUP | KEY_CPAD_UP)) != 0;
        bool navDownB = (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) != 0;
        bool navLeft  = (kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) != 0;
        bool navRight = (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) != 0;
        touchPosition touch;
        bool touched = (kDown & KEY_TOUCH) != 0;
        if (touched) hidTouchRead(&touch);

        if ((kDown & KEY_SELECT) && s_screen != SCR_OPTIONS) {
            s_options_return = s_screen;
            s_screen = SCR_OPTIONS;
            s_menu_cursor = 0;
        }

        // --- UPDATE ---
        if (s_screen == SCR_TITLE) {
            UiButton btns[3] = {
                { 50,  62, 220, 42, s_save.unlocked_level > 1 ? "CONTINUER" : "JOUER" },
                { 50, 114, 220, 42, "NIVEAUX (1-5)" },
                { 50, 166, 220, 42, "OPTIONS" },
            };
            if (navDownB) s_menu_cursor = (s_menu_cursor + 1) % 3;
            if (navUp)    s_menu_cursor = (s_menu_cursor + 2) % 3;

            int act = (kDown & KEY_A) ? s_menu_cursor : -1;
            if (touched) {
                for (int i = 0; i < 3; i++) {
                    if (ui_button_hit(&btns[i], touch.px, touch.py)) act = i;
                }
            }
            if (act == 0) {
                start_level(s_save.unlocked_level);
            } else if (act == 1) {
                s_screen = SCR_SELECT;
                load_preview(s_select_id);
            } else if (act == 2) {
                s_options_return = SCR_TITLE;
                s_screen = SCR_OPTIONS;
                s_menu_cursor = 0;
            }
        } else if (s_screen == SCR_SELECT) {
            if (navRight || navDownB || (kDown & KEY_R)) { if (s_select_id < TOTAL_MAIN_LEVELS) s_select_id++; }
            if (navLeft  || navUp    || (kDown & KEY_L)) { if (s_select_id > 1) s_select_id--; }
            load_preview(s_select_id);

            if (kDown & KEY_B) s_screen = SCR_TITLE;
            if ((kDown & KEY_A) && s_select_id <= s_save.unlocked_level) {
                start_level(s_select_id);
            }
            if (touched) {
                UiButton back_btn = { 10, 200, 96, 30, "RETOUR" };
                if (ui_button_hit(&back_btn, touch.px, touch.py)) {
                    s_screen = SCR_TITLE;
                } else {
                    for (int i = 0; i < TOTAL_MAIN_LEVELS; i++) {
                        UiButton cell = { 40, 34 + i * 32, 240, 28, NULL };
                        if (ui_button_hit(&cell, touch.px, touch.py)) {
                            s_select_id = i + 1;
                            load_preview(s_select_id);
                            if (s_select_id <= s_save.unlocked_level) {
                                start_level(s_select_id);
                            }
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
            } else if (kDown & KEY_B) {
                start_level(s_game.cur_level);
            } else if (kDown & KEY_X) {
                s_save.show_hud = !s_save.show_hud;
            } else if (kDown & KEY_Y) {
                if (s_game.cur_level < TOTAL_MAIN_LEVELS) {
                    if (s_save.unlocked_level <= s_game.cur_level) {
                        s_save.unlocked_level = s_game.cur_level + 1;
                        save_write(&s_save);
                    }
                    start_level(s_game.cur_level + 1);
                }
            } else {
                if (touched) {
                    UiButton b_pause   = { 10,  196, 94, 34, "PAUSE" };
                    UiButton b_restart = { 113, 196, 94, 34, "RECOMM." };
                    UiButton b_menu    = { 216, 196, 94, 34, "MENU" };
                    if (ui_button_hit(&b_pause, touch.px, touch.py)) {
                        audio_play_sfx(SFX_PAUSE);
                        s_screen = SCR_PAUSE;
                        s_menu_cursor = 0;
                    } else if (ui_button_hit(&b_restart, touch.px, touch.py)) {
                        start_level(s_game.cur_level);
                    } else if (ui_button_hit(&b_menu, touch.px, touch.py)) {
                        s_save.total_deaths = s_game.deaths;
                        save_write(&s_save);
                        s_screen = SCR_TITLE;
                    }
                }

                if (s_screen == SCR_PLAYING) {
                    uint32_t ev = game_update(&s_game, read_movement_input(kHeld), get_speed_multiplier());
                    if (ev & GAME_EVT_DIE)  audio_play_sfx(SFX_DIE);
                    if (ev & GAME_EVT_COIN) audio_play_sfx(SFX_COIN);
                    if (ev & GAME_EVT_KEY)  audio_play_sfx(SFX_KEY);
                    if (ev & GAME_EVT_ROOM_WARP) {
                        if (load_room(s_game.cur_level, s_game.warp_col, s_game.warp_row)) {
                            game_enter_room(&s_game, &s_level, s_game.warp_col, s_game.warp_row, s_game.warp_check_idx);
                        }
                    }
                    if (ev & GAME_EVT_WIN) {
                        s_save.total_deaths = s_game.deaths;
                        int idx = s_game.cur_level - 1;
                        if (idx >= 0 && idx < TOTAL_MAIN_LEVELS) {
                            uint32_t ld = s_game.level_deaths > 32767 ? 32767 : s_game.level_deaths;
                            if (s_save.best_deaths[idx] < 0 || (int)ld < s_save.best_deaths[idx]) {
                                s_save.best_deaths[idx] = (int16_t)ld;
                            }
                        }
                        if (s_game.cur_level < TOTAL_MAIN_LEVELS) {
                            if (s_save.unlocked_level <= s_game.cur_level) {
                                s_save.unlocked_level = s_game.cur_level + 1;
                            }
                            save_write(&s_save);
                            audio_play_sfx(SFX_WIN);
                            start_level(s_game.cur_level + 1);
                        } else {
                            save_write(&s_save);
                            audio_play_sfx(SFX_WIN_GAME);
                            s_screen = SCR_VICTORY;
                        }
                    }
                }
            }
        } else if (s_screen == SCR_PAUSE) {
            UiButton btns[4] = {
                { 50,  48, 220, 36, "REPRENDRE" },
                { 50,  92, 220, 36, "RECOMMENCER" },
                { 50, 136, 220, 36, "OPTIONS" },
                { 50, 180, 220, 36, "QUITTER AU MENU" },
            };
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
                    start_level(s_game.cur_level);
                } else if (act == 2) {
                    s_options_return = SCR_PAUSE;
                    s_screen = SCR_OPTIONS;
                    s_menu_cursor = 0;
                } else if (act == 3) {
                    s_save.total_deaths = s_game.deaths;
                    save_write(&s_save);
                    s_screen = SCR_TITLE;
                }
            }
        } else if (s_screen == SCR_OPTIONS) {
            if (navDownB) s_menu_cursor = (s_menu_cursor + 1) % 5;
            if (navUp)    s_menu_cursor = (s_menu_cursor + 4) % 5;
            if (kDown & (KEY_B | KEY_SELECT)) {
                save_write(&s_save);
                s_screen = s_options_return;
            } else {
                int act = (kDown & KEY_A) ? s_menu_cursor : -1;
                if (touched) {
                    for (int i = 0; i < 5; i++) {
                        UiButton b = { 35, 42 + i * 38, 250, 32, NULL };
                        if (ui_button_hit(&b, touch.px, touch.py)) act = i;
                    }
                }
                if (act == 0) {
                    s_save.muted = !s_save.muted;
                    audio_set_muted(s_save.muted != 0);
                } else if (act == 1) {
                    s_save.player_skin = (s_save.player_skin + 1) % 6;
                } else if (act == 2) {
                    s_save.show_timer = !s_save.show_timer;
                } else if (act == 3) {
                    s_save.speed_mode = (s_save.speed_mode + 1) % 3;
                } else if (act == 4) {
                    save_write(&s_save);
                    s_screen = s_options_return;
                }
            }
        } else if (s_screen == SCR_VICTORY) {
            if (kDown & (KEY_A | KEY_B | KEY_START | KEY_TOUCH)) {
                s_screen = SCR_TITLE;
            }
        }

        // --- RENDER ---
        float slider = osGet3DSliderState();
        render_begin_frame();
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        int eyes = (slider > 0.0f) ? 2 : 1;
        for (int eye = 0; eye < eyes; eye++) {
            float d3d = (eye == 0) ? -slider : slider;
            C3D_RenderTarget* target = (eye == 0) ? top_left : top_right;
            C2D_TargetClear(target, clrBgBot);
            C2D_SceneBegin(target);
            render_draw_bg_gradient(TOP_W, TOP_H, clrBgTop, clrBgBot);

            if (s_screen == SCR_TITLE) {
                render_draw_card(34, 28, 332, 112, C2D_Color32(0x16, 0x1A, 0x32, 0xF0), clrBorder);
                render_draw_text_centered_shadow(TOP_W * 0.5f, 40.0f, 0.85f, clrWhite, "THE WORLD'S");
                render_draw_text_centered_shadow(TOP_W * 0.5f, 72.0f, 1.05f, clrRed, "HARDEST GAME 3");
                render_draw_text_centered(TOP_W * 0.5f, 114.0f, 0.56f, clrCyan, "Nintendo 3DS - 5 Mondes (48 Salles)");

                render_draw_card(34, 150, 332, 34, clrPanel, clrBorder);
                char stats[64];
                snprintf(stats, sizeof(stats), "Morts : %lu   |   Progression : %u / 5",
                         (unsigned long)s_save.total_deaths, s_save.unlocked_level);
                render_draw_text_centered_shadow(TOP_W * 0.5f, 159.0f, 0.58f, clrYellow, stats);

                render_draw_text_centered(TOP_W * 0.5f, 194.0f, 0.46f,
                    s_is_n3ds ? clrGreen : clrYellow,
                    s_is_n3ds ? "Mode : New 3DS (804 MHz + Cache L2)" : "Mode : Old 3DS (268 MHz Optimise)");
            } else if (s_screen == SCR_SELECT) {
                render_level_top(&s_preview_level, NULL, s_save.player_skin, d3d, true);
                char title[64];
                int bd = s_save.best_deaths[s_select_id - 1];
                if (bd >= 0) {
                    snprintf(title, sizeof(title), "MONDE %d / 5  (Record: %d morts)", s_select_id, bd);
                } else {
                    snprintf(title, sizeof(title), "MONDE %d / 5  (Pieces: %u)", s_select_id, WHG3_LEVEL_COINS[s_select_id - 1]);
                }
                C2D_DrawRectSolid(0, 0, 0.8f, TOP_W, 24, C2D_Color32(10, 12, 24, 200));
                C2D_DrawRectSolid(0, 23, 0.8f, TOP_W, 1.5f, clrBorder);
                render_draw_text_centered_shadow(TOP_W * 0.5f, 4.0f, 0.50f, clrYellow, title);
            } else if (s_screen == SCR_VICTORY) {
                render_draw_card(30, 44, 340, 150, clrPanel, clrGreen);
                render_draw_text_centered_shadow(TOP_W * 0.5f, 60.0f, 0.95f, clrGreen, "FELICITATIONS !");
                render_draw_text_centered_shadow(TOP_W * 0.5f, 105.0f, 0.58f, clrWhite,
                    "Vous avez termine les 5 mondes !");
                char stats[64];
                snprintf(stats, sizeof(stats), "Morts totales : %lu", (unsigned long)s_save.total_deaths);
                render_draw_text_centered_shadow(TOP_W * 0.5f, 145.0f, 0.56f, clrYellow, stats);
            } else if (s_level_loaded) {
                render_level_top(&s_level, &s_game, s_save.player_skin, d3d, false);
            }
        }

        // Bottom screen
        C2D_TargetClear(bot, clrBgBot);
        C2D_SceneBegin(bot);
        render_draw_bg_gradient(BOT_W, BOT_H, clrBgTop, clrBgBot);

        if (s_screen == SCR_TITLE) {
            render_draw_card(16, 12, 288, 32, clrPanel, clrBorder);
            render_draw_text_centered_shadow(BOT_W * 0.5f, 19.0f, 0.58f, clrYellow, "MENU PRINCIPAL");
            UiButton btns[3] = {
                { 50,  62, 220, 42, s_save.unlocked_level > 1 ? "CONTINUER" : "JOUER" },
                { 50, 114, 220, 42, "MONDES (1-5)" },
                { 50, 166, 220, 42, "OPTIONS" },
            };
            for (int i = 0; i < 3; i++) {
                bool sel = (i == s_menu_cursor);
                render_draw_button(&btns[i], sel ? clrSelTop : clrBtnTop,
                                   sel ? clrSelBot : clrBtnBot,
                                   sel ? clrYellow : clrBorder, clrWhite);
            }
        } else if (s_screen == SCR_SELECT) {
            render_draw_text_centered_shadow(BOT_W * 0.5f, 6.0f, 0.52f, clrYellow, "CHOISIR UN MONDE");
            for (int i = 0; i < TOTAL_MAIN_LEVELS; i++) {
                int id = i + 1;
                bool unlocked = (id <= s_save.unlocked_level);
                bool beaten = (s_save.best_deaths[i] >= 0);
                bool selected = (id == s_select_id);
                char lbl[32];
                snprintf(lbl, sizeof(lbl), "Monde %d (%u pieces)", id, WHG3_LEVEL_COINS[i]);
                UiButton cell = { 40, 34 + i * 32, 240, 28, lbl };
                u32 bt = selected ? clrSelTop : (beaten ? clrBeatTop : (unlocked ? clrBtnTop : clrLockTop));
                u32 bb = selected ? clrSelBot : (beaten ? clrBeatBot : (unlocked ? clrBtnBot : clrLockBot));
                u32 bd = selected ? clrYellow : (beaten ? clrGreen : (unlocked ? clrBorder : clrGray));
                render_draw_button(&cell, bt, bb, bd, unlocked ? clrWhite : clrGray);
            }
            UiButton back_btn = { 10, 200, 96, 30, "RETOUR" };
            render_draw_button(&back_btn, clrBtnTop, clrBtnBot, clrBorder, clrWhite);
            render_draw_text(116, 207, 0.44f, clrGray, "A: Jouer   B: Retour");
        } else if (s_screen == SCR_PLAYING && s_level_loaded) {
            if (s_save.show_hud) {
                render_draw_card(8, 8, 304, 178, clrPanel, clrBorder);
                char buf[64];
                snprintf(buf, sizeof(buf), "MONDE %u/5 (%c%u)", s_game.cur_level, s_game.cur_col, s_game.cur_row);
                render_draw_card(16, 15, 148, 26, clrBtnBot, clrYellow);
                render_draw_text_centered_shadow(90.0f, 20.0f, 0.48f, clrYellow, buf);

                snprintf(buf, sizeof(buf), "Morts : %lu", (unsigned long)s_game.deaths);
                render_draw_text_shadow(18, 50, 0.52f, clrRed, buf);

                uint16_t tot_c = WHG3_LEVEL_COINS[s_game.cur_level - 1];
                snprintf(buf, sizeof(buf), "Pieces : %u / %u", s_game.coins_collected_count, tot_c);
                render_draw_text_shadow(18, 76, 0.52f, clrGreen, buf);

                C2D_DrawRectSolid(18, 98, 0.1f, 144, 6, C2D_Color32(15, 18, 32, 255));
                float ratio = tot_c > 0 ? (float)s_game.coins_collected_count / (float)tot_c : 1.0f;
                if (ratio > 1.0f) ratio = 1.0f;
                C2D_DrawRectSolid(18, 98, 0.15f, 144.0f * ratio, 6, clrYellow);

                if (s_save.show_timer) {
                    u32 ds = (s_game.level_ticks * 10) / 60;
                    snprintf(buf, sizeof(buf), "Temps : %lu:%02lu.%lu",
                             (unsigned long)(ds / 600), (unsigned long)((ds / 10) % 60), (unsigned long)(ds % 10));
                    render_draw_text_shadow(18, 112, 0.50f, clrCyan, buf);
                }

                render_draw_text(18, 142, 0.38f, clrGray, "B: Recommencer   X: HUD");
                render_draw_text(18, 158, 0.38f, clrGray, "START: Pause   SELECT: Opt.");

                render_minimap(&s_level, &s_game, 178.0f, 24.0f, 124.0f, 88.0f);
                render_draw_text_centered(240.0f, 118.0f, 0.38f, clrGray, "SALLE");

                UiButton b_pause   = { 10,  196, 94, 34, "PAUSE" };
                UiButton b_restart = { 113, 196, 94, 34, "RECOMM." };
                UiButton b_menu    = { 216, 196, 94, 34, "MENU" };
                render_draw_button(&b_pause, clrBtnTop, clrBtnBot, clrCyan, clrWhite);
                render_draw_button(&b_restart, C2D_Color32(0x70, 0x48, 0x20, 0xFF),
                                   C2D_Color32(0x4A, 0x2E, 0x12, 0xFF), clrYellow, clrWhite);
                render_draw_button(&b_menu, clrBtnTop, clrBtnBot, clrBorder, clrWhite);
            } else {
                render_draw_text_centered_shadow(BOT_W * 0.5f, 110.0f, 0.5f, clrGray, "HUD masque (X pour afficher)");
            }
        } else if (s_screen == SCR_PAUSE) {
            render_draw_card(16, 8, 288, 30, clrPanel, clrYellow);
            render_draw_text_centered_shadow(BOT_W * 0.5f, 14.0f, 0.58f, clrYellow, "PAUSE");
            UiButton btns[4] = {
                { 50,  48, 220, 36, "REPRENDRE" },
                { 50,  92, 220, 36, "RECOMMENCER" },
                { 50, 136, 220, 36, "OPTIONS" },
                { 50, 180, 220, 36, "QUITTER AU MENU" },
            };
            for (int i = 0; i < 4; i++) {
                bool sel = (i == s_menu_cursor);
                render_draw_button(&btns[i], sel ? clrSelTop : clrBtnTop,
                                   sel ? clrSelBot : clrBtnBot,
                                   sel ? clrYellow : clrBorder, clrWhite);
            }
        } else if (s_screen == SCR_OPTIONS) {
            render_draw_card(16, 6, 288, 28, clrPanel, clrCyan);
            render_draw_text_centered_shadow(BOT_W * 0.5f, 12.0f, 0.54f, clrCyan, "OPTIONS");
            char l0[32], l1[32], l2[32], l3[40];
            snprintf(l0, sizeof(l0), "SON : %s", s_save.muted ? "MUET" : "ACTIVE");
            snprintf(l1, sizeof(l1), "COULEUR JOUEUR : %d/6", s_save.player_skin + 1);
            snprintf(l2, sizeof(l2), "CHRONOMETRE : %s", s_save.show_timer ? "OUI" : "NON");
            snprintf(l3, sizeof(l3), "VITESSE CUBE : %s", get_speed_label());
            const char* labels[5] = { l0, l1, l2, l3, "RETOUR" };
            for (int i = 0; i < 5; i++) {
                UiButton b = { 35, 42 + i * 38, 250, 32, labels[i] };
                bool sel = (i == s_menu_cursor);
                render_draw_button(&b, sel ? clrSelTop : clrBtnTop,
                                   sel ? clrSelBot : clrBtnBot,
                                   sel ? clrYellow : clrBorder, clrWhite);
            }
        } else if (s_screen == SCR_VICTORY) {
            render_draw_text_centered_shadow(BOT_W * 0.5f, 104.0f, 0.58f, clrWhite, "Appuyez sur A pour continuer");
        }

        C3D_FrameEnd(0);
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
