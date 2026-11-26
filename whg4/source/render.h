#ifndef RENDER_H
#define RENDER_H

#include "game.h"
#include <citro2d.h>

#define TOP_W 400
#define TOP_H 240
#define BOT_W 320
#define BOT_H 240

typedef struct {
    int x, y, w, h;
    const char* label;
} UiButton;

bool render_init(void);
void render_exit(void);
void render_begin_frame(void);

void render_draw_bg_gradient(float w, float h, u32 top_col, u32 bot_col);
void render_draw_card(float x, float y, float w, float h, u32 bg_col, u32 border_col);
void render_draw_text(float x, float y, float scale, u32 color, const char* text);
void render_draw_text_shadow(float x, float y, float scale, u32 color, const char* text);
void render_draw_text_centered(float cx, float y, float scale, u32 color, const char* text);
void render_draw_text_centered_shadow(float cx, float y, float scale, u32 color, const char* text);
void render_draw_button(const UiButton* btn, u32 bg_top, u32 bg_bot, u32 border_color, u32 text_color);
bool ui_button_hit(const UiButton* btn, int tx, int ty);

void render_title_anim(float time_sec, float depth_3d);
void render_level_top(const LevelData* lvl, const GameState* gs,
                      int player_skin, float depth_3d, float zoom, bool preview_only);
void render_minimap(const LevelData* lvl, const GameState* gs,
                    float mx, float my, float mw, float mh);

#endif // RENDER_H
