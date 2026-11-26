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

static float get_zoom_factor(void) {
    if (s_save.zoom_mode == 1) return 1.50f;
    if (s_save.zoom_mode == 2) return 2.00f;
    return 1.00f;
}

static const char* get_zoom_label(void) {
    if (s_save.zoom_mode == 1) return "SUIVI (1.5x)";
    if (s_save.zoom_mode == 2) return "PROCHE (2.0x)";
    return "GLOBAL (1.0x)";
}

static void load_preview(int id) {
    if (id == s_preview_id) return;
    level_free(&s_preview_level);
    const Whg4StartFinish* st = &WHG4_STARTS[id - 1];
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
    const Whg4StartFinish* st = &WHG4_STARTS[id - 1];
    if (!load_room(id, st->col, st->row)) return false;
    game_init_level(&s_game, (uint8_t)id, &s_level, s_save.total_deaths);
    s_screen = SCR_PLAYING;
    return true;
}

static GameInput read_movement_input(u32 kHeld) {
    GameInput in = { 0, 0 };
    circlePosition cpad;
    hidCircleRead(&cpad);
    if ((kHeld & KEY_LEFT)  || cpad.dx < -35) in.dx = -1;
    if ((kHeld & KEY_RIGHT) || cpad.dx >  35) in.dx =  1;
    if ((kHeld & KEY_UP)    || cpad.dy >  35) in.dy = -1;
    if ((kHeld & KEY_DOWN)  || cpad.dy < -35) in.dy =  1;
    return in;
}

int main(void) {
    gfxInitDefault();
    osSetSpeedupEnable(true);
    APT_CheckNew3DS(&s_is_n3ds);
    romfsInit();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* topL = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* topR = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    C3D_RenderTarget* bot  = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    gfxSet3D(true);

    render_init();
    audio_init();
    save_load(&s_save);
    audio_set_muted(s_save.muted != 0);

    float time_sec = 0.0f;
    u32 touch_cooldown = 0;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        touchPosition touch = { 0, 0 };
        bool touched = false;
        if (touch_cooldown > 0) touch_cooldown--;
        if ((kDown & KEY_TOUCH) && touch_cooldown == 0) {
            hidTouchRead(&touch);
            touched = true;
            touch_cooldown = 8;
        }

        if ((kDown & KEY_START) && s_screen == SCR_TITLE) break;

        time_sec += 1.0f / 60.0f;
        audio_update();

        bool navUp    = (kDown & KEY_UP)    != 0;
        bool navDown  = (kDown & KEY_DOWN)  != 0;
        bool navLeft  = (kDown & KEY_LEFT)  != 0;
        bool navRight = (kDown & KEY_RIGHT) != 0;
        bool navOk    = (kDown & KEY_A)     != 0;
        bool navBack  = (kDown & KEY_B)     != 0;

        if (s_screen == SCR_TITLE) {
            if (navDown) s_menu_cursor = (s_menu_cursor + 1) % 3;
            if (navUp)   s_menu_cursor = (s_menu_cursor + 2) % 3;

            UiButton btn_play   = { 50,  38, 220, 46, "JOUER" };
            UiButton btn_select = { 50,  94, 220, 46, "CHOIX MONDE" };
            UiButton btn_opt    = { 50, 150, 220, 46, "OPTIONS" };

            int act = -1;
            if (navOk) act = s_menu_cursor;
            if (touched) {
                if (ui_button_hit(&btn_play, touch.px, touch.py))        act = 0;
                else if (ui_button_hit(&btn_select, touch.px, touch.py)) act = 1;
                else if (ui_button_hit(&btn_opt, touch.px, touch.py))    act = 2;
            }
            if (act == 0) {
                start_level(s_save.unlocked_level);
            } else if (act == 1) {
                s_select_id = s_save.unlocked_level;
                load_preview(s_select_id);
                s_screen = SCR_SELECT;
            } else if (act == 2) {
                s_options_return = SCR_TITLE;
                s_menu_cursor = 0;
                s_screen = SCR_OPTIONS;
            }
        }
        else if (s_screen == SCR_SELECT) {
            if (navLeft)  s_select_id--;
            if (navRight) s_select_id++;
            if (s_select_id < 1) s_select_id = TOTAL_LEVELS;
            if (s_select_id > TOTAL_LEVELS) s_select_id = 1;
            load_preview(s_select_id);

            UiButton btn_prev = {  16,  52,  54, 48, "<" };
            UiButton btn_next = { 250,  52,  54, 48, ">" };
            UiButton btn_go   = {  40, 128, 240, 44, "LANCER MONDE" };
            UiButton btn_back = {  40, 182, 240, 38, "RETOUR" };

            if (touched) {
                if (ui_button_hit(&btn_prev, touch.px, touch.py)) {
                    s_select_id = (s_select_id <= 1) ? TOTAL_LEVELS : s_select_id - 1;
                    load_preview(s_select_id);
                } else if (ui_button_hit(&btn_next, touch.px, touch.py)) {
                    s_select_id = (s_select_id >= TOTAL_LEVELS) ? 1 : s_select_id + 1;
                    load_preview(s_select_id);
                } else if (ui_button_hit(&btn_go, touch.px, touch.py)) {
                    start_level(s_select_id);
                } else if (ui_button_hit(&btn_back, touch.px, touch.py)) {
                    s_screen = SCR_TITLE;
                }
            }
            if (navOk)   start_level(s_select_id);
            if (navBack) s_screen = SCR_TITLE;
        }
        else if (s_screen == SCR_PLAYING) {
            if (kDown & KEY_L) {
                s_save.zoom_mode = (s_save.zoom_mode + 2) % 3;
                save_write(&s_save);
            }
            if (kDown & KEY_R) {
                s_save.zoom_mode = (s_save.zoom_mode + 1) % 3;
                save_write(&s_save);
            }
            if ((kDown & KEY_START) || (kDown & KEY_SELECT)) {
                audio_play_sfx(SFX_PAUSE);
                s_menu_cursor = 0;
                s_screen = SCR_PAUSE;
            } else if (kDown & KEY_B) {
                start_level(s_game.cur_level);
            } else if (s_level_loaded) {
                GameInput in = read_movement_input(kHeld);
                uint32_t evt = game_update(&s_game, in, get_speed_multiplier());
                if (evt & GAME_EVT_DIE)       { audio_play_sfx(SFX_DIE); s_save.total_deaths = s_game.deaths; save_write(&s_save); }
                if (evt & GAME_EVT_DIE_WATER) { audio_play_sfx(SFX_DIE_WATER); s_save.total_deaths = s_game.deaths; save_write(&s_save); }
                if (evt & GAME_EVT_DIE_PIT)   { audio_play_sfx(SFX_DIE_PIT); s_save.total_deaths = s_game.deaths; save_write(&s_save); }
                if (evt & GAME_EVT_COIN)       audio_play_sfx(SFX_COIN);
                if (evt & GAME_EVT_DIAMOND)    audio_play_sfx(SFX_DIAMOND);
                if (evt & GAME_EVT_KEY)        audio_play_sfx(SFX_KEY);
                if (evt & GAME_EVT_CHECKPOINT) audio_play_sfx(SFX_CHECK);
                if (evt & GAME_EVT_ROOM_WARP) {
                    if (load_room(s_game.cur_level, s_game.warp_col, s_game.warp_row)) {
                        game_enter_room(&s_game, &s_level, s_game.warp_col, s_game.warp_row,
                                        s_game.warp_place_at_spawn, s_game.warp_check_idx);
                    }
                }
                if (evt & GAME_EVT_WIN) {
                    int lv = s_game.cur_level;
                    if (s_save.best_deaths[lv - 1] < 0 || (int)s_game.level_deaths < s_save.best_deaths[lv - 1]) {
                        s_save.best_deaths[lv - 1] = (int16_t)s_game.level_deaths;
                    }
                    if (lv + 1 <= TOTAL_LEVELS && (uint16_t)(lv + 1) > s_save.unlocked_level) {
                        s_save.unlocked_level = (uint16_t)(lv + 1);
                    }
                    s_save.total_deaths = s_game.deaths;
                    save_write(&s_save);
                    if (lv >= TOTAL_LEVELS) {
                        audio_play_sfx(SFX_WIN_GAME);
                        s_screen = SCR_VICTORY;
                    } else {
                        audio_play_sfx(SFX_WIN);
                        start_level(lv + 1);
                    }
                }
            }
            if (touched) {
                UiButton btn_pause = { 210, 186, 98, 40, "PAUSE" };
                if (ui_button_hit(&btn_pause, touch.px, touch.py)) {
                    audio_play_sfx(SFX_PAUSE);
                    s_menu_cursor = 0;
                    s_screen = SCR_PAUSE;
                }
            }
        }
        else if (s_screen == SCR_PAUSE) {
            if (navDown) s_menu_cursor = (s_menu_cursor + 1) % 4;
            if (navUp)   s_menu_cursor = (s_menu_cursor + 3) % 4;
            UiButton b0 = { 50,  26, 220, 42, "REPRENDRE" };
            UiButton b1 = { 50,  76, 220, 42, "RECOMMENCER" };
            UiButton b2 = { 50, 126, 220, 42, "OPTIONS" };
            UiButton b3 = { 50, 176, 220, 42, "MENU TITRE" };
            int act = navOk ? s_menu_cursor : -1;
            if (navBack) act = 0;
            if (touched) {
                if (ui_button_hit(&b0, touch.px, touch.py)) act = 0;
                else if (ui_button_hit(&b1, touch.px, touch.py)) act = 1;
                else if (ui_button_hit(&b2, touch.px, touch.py)) act = 2;
                else if (ui_button_hit(&b3, touch.px, touch.py)) act = 3;
            }
            if (act == 0) s_screen = SCR_PLAYING;
            else if (act == 1) start_level(s_game.cur_level);
            else if (act == 2) { s_options_return = SCR_PAUSE; s_menu_cursor = 0; s_screen = SCR_OPTIONS; }
            else if (act == 3) s_screen = SCR_TITLE;
        }
        else if (s_screen == SCR_OPTIONS) {
            if (navDown) s_menu_cursor = (s_menu_cursor + 1) % 6;
            if (navUp)   s_menu_cursor = (s_menu_cursor + 5) % 6;
            UiButton b0 = { 26,  14, 268, 32, NULL };
            UiButton b1 = { 26,  50, 268, 32, NULL };
            UiButton b2 = { 26,  86, 268, 32, NULL };
            UiButton b3 = { 26, 122, 268, 32, NULL };
            UiButton b4 = { 26, 158, 268, 32, NULL };
            UiButton b5 = { 26, 196, 268, 34, "RETOUR" };

            int act = navOk ? s_menu_cursor : -1;
            if (navBack) act = 5;
            if (touched) {
                if (ui_button_hit(&b0, touch.px, touch.py)) act = 0;
                else if (ui_button_hit(&b1, touch.px, touch.py)) act = 1;
                else if (ui_button_hit(&b2, touch.px, touch.py)) act = 2;
                else if (ui_button_hit(&b3, touch.px, touch.py)) act = 3;
                else if (ui_button_hit(&b4, touch.px, touch.py)) act = 4;
                else if (ui_button_hit(&b5, touch.px, touch.py)) act = 5;
            }
            if (act == 0) { s_save.muted = !s_save.muted; audio_set_muted(s_save.muted != 0); save_write(&s_save); }
            else if (act == 1) { s_save.player_skin = (s_save.player_skin + 1) % 6; save_write(&s_save); }
            else if (act == 2) { s_save.show_timer = !s_save.show_timer; save_write(&s_save); }
            else if (act == 3) { s_save.speed_mode = (s_save.speed_mode + 1) % 3; save_write(&s_save); }
            else if (act == 4) { s_save.zoom_mode = (s_save.zoom_mode + 1) % 3; save_write(&s_save); }
            else if (act == 5) { s_screen = s_options_return; s_menu_cursor = 0; }
        }
        else if (s_screen == SCR_VICTORY) {
            UiButton b0 = { 50, 165, 220, 44, "MENU TITRE" };
            if (navOk || navBack || (touched && ui_button_hit(&b0, touch.px, touch.py))) {
                s_screen = SCR_TITLE;
            }
        }

        // Render
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        render_begin_frame();

        float slider = osGet3DSliderState();
        int num_eyes = (slider > 0.05f) ? 2 : 1;
        for (int eye = 0; eye < num_eyes; eye++) {
            float d3d = (num_eyes == 1) ? 0.0f : ((eye == 0) ? -slider * 4.0f : slider * 4.0f);
            C3D_RenderTarget* tgt = (eye == 0) ? topL : topR;
            C2D_SceneBegin(tgt);
            C2D_TargetClear(tgt, C2D_Color32(14, 16, 30, 255));

            if (s_screen == SCR_TITLE) {
                render_draw_bg_gradient(TOP_W, TOP_H, C2D_Color32(18, 22, 46, 255), C2D_Color32(34, 24, 62, 255));
                render_title_anim(time_sec, d3d);
                render_draw_text_centered_shadow(200.0f + d3d, 38.0f, 0.85f, C2D_Color32(255, 255, 255, 255), "THE WORLD'S");
                render_draw_text_centered_shadow(200.0f + d3d, 68.0f, 1.05f, C2D_Color32(255, 70, 70, 255), "HARDEST GAME 4");
                render_draw_text_centered(200.0f, 114.0f, 0.45f, C2D_Color32(200, 210, 255, 255), "5 Mondes  |  60 Salles  |  60 FPS");
                const char* hw = s_is_n3ds ? "New 3DS : 804 MHz + Cache L2" : "Old 3DS : 268 MHz Optimise";
                u32 hcol = s_is_n3ds ? C2D_Color32(90, 235, 130, 255) : C2D_Color32(255, 215, 90, 255);
                render_draw_text_centered(200.0f, 204.0f, 0.42f, hcol, hw);
            } else if (s_screen == SCR_SELECT) {
                render_level_top(&s_preview_level, NULL, s_save.player_skin, d3d, 1.0f, true);
            } else if (s_screen == SCR_VICTORY) {
                render_draw_bg_gradient(TOP_W, TOP_H, C2D_Color32(20, 45, 35, 255), C2D_Color32(15, 20, 35, 255));
                render_draw_text_centered_shadow(200.0f + d3d, 70.0f, 0.95f, C2D_Color32(255, 225, 70, 255), "FELICITATIONS !");
                render_draw_text_centered(200.0f, 115.0f, 0.55f, C2D_Color32(255, 255, 255, 255), "Vous avez termine WHG 4 !");
                char buf[64];
                snprintf(buf, sizeof(buf), "Morts totales : %lu", (unsigned long)s_save.total_deaths);
                render_draw_text_centered(200.0f, 150.0f, 0.55f, C2D_Color32(255, 130, 130, 255), buf);
            } else {
                render_level_top(&s_level, &s_game, s_save.player_skin, d3d, get_zoom_factor(), false);
            }
        }

        // Bottom screen
        C2D_SceneBegin(bot);
        C2D_TargetClear(bot, C2D_Color32(16, 18, 34, 255));
        render_draw_bg_gradient(BOT_W, BOT_H, C2D_Color32(20, 24, 48, 255), C2D_Color32(12, 14, 28, 255));

        u32 btnTop = C2D_Color32(55, 68, 125, 255);
        u32 btnBot = C2D_Color32(36, 44, 88, 255);
        u32 btnSelTop = C2D_Color32(85, 108, 185, 255);
        u32 btnSelBot = C2D_Color32(52, 68, 132, 255);
        u32 brdNorm = C2D_Color32(110, 130, 205, 255);
        u32 brdSel  = C2D_Color32(255, 220, 90, 255);
        u32 clrWhite = C2D_Color32(255, 255, 255, 255);
        u32 clrGray  = C2D_Color32(175, 185, 215, 255);

        if (s_screen == SCR_TITLE) {
            UiButton b0 = { 50,  38, 220, 46, "JOUER" };
            UiButton b1 = { 50,  94, 220, 46, "CHOIX MONDE" };
            UiButton b2 = { 50, 150, 220, 46, "OPTIONS" };
            render_draw_button(&b0, s_menu_cursor==0?btnSelTop:btnTop, s_menu_cursor==0?btnSelBot:btnBot, s_menu_cursor==0?brdSel:brdNorm, clrWhite);
            render_draw_button(&b1, s_menu_cursor==1?btnSelTop:btnTop, s_menu_cursor==1?btnSelBot:btnBot, s_menu_cursor==1?brdSel:brdNorm, clrWhite);
            render_draw_button(&b2, s_menu_cursor==2?btnSelTop:btnTop, s_menu_cursor==2?btnSelBot:btnBot, s_menu_cursor==2?brdSel:brdNorm, clrWhite);
        } else if (s_screen == SCR_SELECT) {
            char title[48];
            snprintf(title, sizeof(title), "MONDE %d / %d", s_select_id, TOTAL_LEVELS);
            render_draw_text_centered_shadow(160.0f, 18.0f, 0.72f, clrWhite, title);
            UiButton bp = {  16,  52,  54, 48, "<" };
            UiButton bn = { 250,  52,  54, 48, ">" };
            UiButton bg = {  40, 128, 240, 44, "LANCER MONDE" };
            UiButton bb = {  40, 182, 240, 38, "RETOUR" };
            render_draw_button(&bp, btnTop, btnBot, brdNorm, clrWhite);
            render_draw_button(&bn, btnTop, btnBot, brdNorm, clrWhite);
            render_draw_button(&bg, btnSelTop, btnSelBot, brdSel, clrWhite);
            render_draw_button(&bb, btnTop, btnBot, brdNorm, clrWhite);
            char best[48];
            if (s_save.best_deaths[s_select_id - 1] >= 0)
                snprintf(best, sizeof(best), "Record : %d morts", s_save.best_deaths[s_select_id - 1]);
            else
                snprintf(best, sizeof(best), "Non termine");
            render_draw_text_centered(160.0f, 70.0f, 0.48f, clrGray, best);
        } else if (s_screen == SCR_PLAYING) {
            render_draw_card(10, 8, 190, 148, C2D_Color32(26, 30, 52, 235), C2D_Color32(95, 110, 175, 255));
            char line[64];
            snprintf(line, sizeof(line), "MONDE %d - SALLE %c%u", s_game.cur_level, s_game.cur_col, s_game.cur_row);
            render_draw_text_shadow(18, 16, 0.50f, clrWhite, line);
            snprintf(line, sizeof(line), "Morts : %lu (Niv: %lu)", (unsigned long)s_game.deaths, (unsigned long)s_game.level_deaths);
            render_draw_text(18, 42, 0.44f, C2D_Color32(255, 130, 130, 255), line);
            snprintf(line, sizeof(line), "Argent : $%u / $%u", game_total_cash(&s_game), WHG4_LEVEL_CASH[s_game.cur_level - 1]);
            render_draw_text(18, 66, 0.44f, C2D_Color32(255, 220, 80, 255), line);
            if (s_save.show_timer) {
                uint32_t sec = s_game.level_ticks / 60;
                snprintf(line, sizeof(line), "Temps : %02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
                render_draw_text(18, 90, 0.44f, clrGray, line);
            }
            snprintf(line, sizeof(line), "Zoom : %s", get_zoom_label());
            render_draw_text(18, 112, 0.40f, C2D_Color32(150, 210, 255, 255), line);
            render_draw_text(18, 134, 0.36f, clrGray, "L/R: Zoom   B: Recommencer");
            render_minimap(&s_level, &s_game, 12, 164, 188, 66);
            UiButton bpause = { 210, 186, 98, 40, "PAUSE" };
            render_draw_button(&bpause, btnTop, btnBot, brdNorm, clrWhite);
        } else if (s_screen == SCR_PAUSE) {
            render_draw_text_centered_shadow(160.0f, 4.0f, 0.65f, clrWhite, "PAUSE");
            UiButton b0 = { 50,  26, 220, 42, "REPRENDRE" };
            UiButton b1 = { 50,  76, 220, 42, "RECOMMENCER" };
            UiButton b2 = { 50, 126, 220, 42, "OPTIONS" };
            UiButton b3 = { 50, 176, 220, 42, "MENU TITRE" };
            UiButton* bs[4] = { &b0, &b1, &b2, &b3 };
            for (int i = 0; i < 4; i++) {
                render_draw_button(bs[i], s_menu_cursor==i?btnSelTop:btnTop, s_menu_cursor==i?btnSelBot:btnBot, s_menu_cursor==i?brdSel:brdNorm, clrWhite);
            }
        } else if (s_screen == SCR_OPTIONS) {
            static const char* skin_names[6] = { "ROUGE", "BLEU", "VERT", "OR", "ROSE", "CYAN" };
            char l0[48], l1[48], l2[48], l3[48], l4[48];
            snprintf(l0, sizeof(l0), "SON : %s", s_save.muted ? "COUPE" : "ACTIVE");
            snprintf(l1, sizeof(l1), "COULEUR : %s", skin_names[s_save.player_skin % 6]);
            snprintf(l2, sizeof(l2), "CHRONOMETRE : %s", s_save.show_timer ? "OUI" : "NON");
            snprintf(l3, sizeof(l3), "VITESSE CUBE : %s", get_speed_label());
            snprintf(l4, sizeof(l4), "ZOOM CAMERA : %s", get_zoom_label());
            UiButton b0 = { 26,  14, 268, 32, l0 };
            UiButton b1 = { 26,  50, 268, 32, l1 };
            UiButton b2 = { 26,  86, 268, 32, l2 };
            UiButton b3 = { 26, 122, 268, 32, l3 };
            UiButton b4 = { 26, 158, 268, 32, l4 };
            UiButton b5 = { 26, 196, 268, 34, "RETOUR" };
            UiButton* bs[6] = { &b0, &b1, &b2, &b3, &b4, &b5 };
            for (int i = 0; i < 6; i++) {
                render_draw_button(bs[i], s_menu_cursor==i?btnSelTop:btnTop, s_menu_cursor==i?btnSelBot:btnBot, s_menu_cursor==i?brdSel:brdNorm, clrWhite);
            }
        } else if (s_screen == SCR_VICTORY) {
            UiButton b0 = { 50, 165, 220, 44, "MENU TITRE" };
            render_draw_button(&b0, btnSelTop, btnSelBot, brdSel, clrWhite);
        }

        C3D_FrameEnd(0);
    }

    game_free(&s_game);
    level_free(&s_level);
    level_free(&s_preview_level);
    audio_exit();
    render_exit();
    C2D_Fini();
    C3D_Fini();
    romfsExit();
    gfxExit();
    return 0;
}
