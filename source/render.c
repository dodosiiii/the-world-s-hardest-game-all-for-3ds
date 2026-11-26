#include "render.h"
#include "sprites.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static C2D_SpriteSheet s_sheet = NULL;
static C2D_TextBuf s_text_buf = NULL;
static C2D_Font s_font = NULL;

bool render_init(void) {
    s_sheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
    if (!s_sheet) return false;
    s_text_buf = C2D_TextBufNew(8192);
    // Load our full 3414-glyph bundled font first (works identically on 3DS & Citra/Azahar)
    s_font = C2D_FontLoad("romfs:/font.bcfnt");
    if (!s_font) {
        s_font = C2D_FontLoadSystem(CFG_REGION_EUR);
    }
    return true;
}

void render_exit(void) {
    if (s_font) { C2D_FontFree(s_font); s_font = NULL; }
    if (s_text_buf) { C2D_TextBufDelete(s_text_buf); s_text_buf = NULL; }
    if (s_sheet) { C2D_SpriteSheetFree(s_sheet); s_sheet = NULL; }
}

void render_begin_frame(void) {
    if (s_text_buf) C2D_TextBufClear(s_text_buf);
}

void render_draw_bg_gradient(float w, float h, u32 top_col, u32 bot_col) {
    C2D_DrawRectangle(0.0f, 0.0f, 0.0f, w, h, top_col, top_col, bot_col, bot_col);
}

void render_draw_card(float x, float y, float w, float h, u32 bg_col, u32 border_col) {
    // Drop shadow
    C2D_DrawRectSolid(x + 2.0f, y + 3.0f, 0.0f, w, h, C2D_Color32(0, 0, 0, 110));
    // Border
    C2D_DrawRectSolid(x, y, 0.0f, w, h, border_col);
    // Inner fill
    C2D_DrawRectSolid(x + 1.5f, y + 1.5f, 0.0f, w - 3.0f, h - 3.0f, bg_col);
    // Subtle top highlight
    C2D_DrawRectSolid(x + 2.0f, y + 2.0f, 0.0f, w - 4.0f, 1.5f, C2D_Color32(255, 255, 255, 35));
}

static void parse_text(C2D_Text* t, const char* text) {
    if (s_font) {
        C2D_TextFontParse(t, s_font, s_text_buf, text);
    } else {
        C2D_TextParse(t, s_text_buf, text);
    }
    C2D_TextOptimize(t);
}

void render_draw_text(float x, float y, float scale, u32 color, const char* text) {
    if (!s_text_buf || !text) return;
    C2D_Text t;
    parse_text(&t, text);
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.5f, scale, scale, color);
}

void render_draw_text_shadow(float x, float y, float scale, u32 color, const char* text) {
    if (!s_text_buf || !text) return;
    C2D_Text t;
    parse_text(&t, text);
    C2D_DrawText(&t, C2D_WithColor, x + 1.5f, y + 1.5f, 0.45f, scale, scale, C2D_Color32(0, 0, 0, 180));
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.5f, scale, scale, color);
}

void render_draw_text_centered(float cx, float y, float scale, u32 color, const char* text) {
    if (!s_text_buf || !text) return;
    C2D_Text t;
    parse_text(&t, text);
    float w = 0, h = 0;
    C2D_TextGetDimensions(&t, scale, scale, &w, &h);
    C2D_DrawText(&t, C2D_WithColor, cx - w * 0.5f, y, 0.5f, scale, scale, color);
}

void render_draw_text_centered_shadow(float cx, float y, float scale, u32 color, const char* text) {
    if (!s_text_buf || !text) return;
    C2D_Text t;
    parse_text(&t, text);
    float w = 0, h = 0;
    C2D_TextGetDimensions(&t, scale, scale, &w, &h);
    float x = cx - w * 0.5f;
    C2D_DrawText(&t, C2D_WithColor, x + 1.5f, y + 1.5f, 0.45f, scale, scale, C2D_Color32(0, 0, 0, 190));
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.5f, scale, scale, color);
}

void render_draw_button(const UiButton* btn, u32 bg_top, u32 bg_bot, u32 border_color, u32 text_color) {
    float x = (float)btn->x, y = (float)btn->y, w = (float)btn->w, h = (float)btn->h;
    // Drop shadow
    C2D_DrawRectSolid(x + 2.0f, y + 3.0f, 0.1f, w, h, C2D_Color32(0, 0, 0, 120));
    // Border
    C2D_DrawRectSolid(x, y, 0.15f, w, h, border_color);
    // Gradient body
    C2D_DrawRectangle(x + 1.5f, y + 1.5f, 0.2f, w - 3.0f, h - 3.0f,
                      bg_top, bg_top, bg_bot, bg_bot);
    // Top gloss line
    C2D_DrawRectSolid(x + 2.5f, y + 2.0f, 0.25f, w - 5.0f, 1.5f, C2D_Color32(255, 255, 255, 55));
    if (btn->label) {
        render_draw_text_centered_shadow(x + w * 0.5f, y + h * 0.5f - 8.5f,
                                         0.62f, text_color, btn->label);
    }
}

bool ui_button_hit(const UiButton* btn, int tx, int ty) {
    return tx >= btn->x && tx < btn->x + btn->w &&
           ty >= btn->y && ty < btn->y + btn->h;
}

static void draw_sprite_centered(size_t idx, float cx, float cy, float scale, float depth, u8 alpha) {
    if (!s_sheet) return;
    C2D_Image img = C2D_SpriteSheetGetImage(s_sheet, idx);
    float w = img.subtex->width * scale;
    float h = img.subtex->height * scale;
    if (alpha >= 255) {
        C2D_DrawImageAt(img, cx - w * 0.5f, cy - h * 0.5f, depth, NULL, scale, scale);
    } else {
        C2D_ImageTint tint;
        C2D_AlphaImageTint(&tint, alpha / 255.0f);
        C2D_DrawImageAt(img, cx - w * 0.5f, cy - h * 0.5f, depth, &tint, scale, scale);
    }
}

void render_title_anim(float time_sec, float depth_3d) {
    // Decorative floating WHG2 shapes in the background
    for (int i = 0; i < 8; i++) {
        float phase = time_sec * (0.7f + (i % 3) * 0.25f) + i * 1.1f;
        float x = 35.0f + i * 48.0f + depth_3d * 2.0f;
        float y = 120.0f + sinf(phase) * 75.0f;
        if (i % 2 == 0) {
            draw_sprite_centered(sprites_enemy_blue_idx, x, y, 1.0f, 0.15f, 110);
        } else {
            draw_sprite_centered(sprites_coin_idx, x, y, 1.0f, 0.15f, 110);
        }
    }
    // Center hero player icon
    draw_sprite_centered(sprites_player_1_idx, TOP_W * 0.5f + depth_3d * 3.0f,
                         22.0f + sinf(time_sec * 2.2f) * 3.0f, 1.1f, 0.3f, 255);
}

void render_level_top(const LevelData* lvl, const GameState* gs,
                      int player_skin, float depth_3d, bool preview_only) {
    if (!lvl) return;

    const float scale = 0.48f;
    const float ox = (TOP_W - lvl->width * scale) * 0.5f;
    const float oy = (TOP_H - lvl->height * scale) * 0.5f;
    const float pw = lvl->width * scale;
    const float ph = lvl->height * scale;

    // Outer frame around playfield
    C2D_DrawRectSolid(ox - 3.0f, 0.0f, 0.0f, pw + 6.0f, TOP_H, C2D_Color32(0x28, 0x2C, 0x52, 0xFF));
    C2D_DrawRectSolid(ox - 1.5f, 0.0f, 0.0f, pw + 3.0f, TOP_H, C2D_Color32(0x10, 0x10, 0x20, 0xFF));

    u32 clrBg1     = C2D_Color32(0xF8, 0xF8, 0xFF, 0xFF);
    u32 clrBg2     = C2D_Color32(0xE2, 0xE2, 0xFA, 0xFF);
    u32 clrWallOut = C2D_Color32(0x18, 0x18, 0x38, 0xFF);
    u32 clrWall    = C2D_Color32(0xB4, 0xB4, 0xFF, 0xFF);
    u32 clrDoor    = C2D_Color32(0xFF, 0xA8, 0x20, 0xFF);
    u32 clrChk     = C2D_Color32(0x88, 0xF4, 0x88, 0xD0);

    // Checkerboard floor: single base rect + odd tiles only (2x fewer GPU quads)
    C2D_DrawRectSolid(ox, oy, 0.04f, pw, ph, clrBg1);
    const int tile = 25;
    for (int ty = 0; ty < lvl->height; ty += tile) {
        for (int tx = 0; tx < lvl->width; tx += tile) {
            if ((((tx / tile) + (ty / tile)) & 1) != 0) {
                C2D_DrawRectSolid(ox + tx * scale, oy + ty * scale, 0.05f,
                                  tile * scale + 0.5f, tile * scale + 0.5f, clrBg2);
            }
        }
    }

    // Checkpoints / Goal zones
    // kind==2 → true goal; kind==1 (no kind==2 present) → goal-as-start pad; kind==3 → mid checkpoint
    bool has_goal2 = false;
    for (uint16_t i = 0; i < lvl->num_checks; i++) {
        if (lvl->checks[i].kind == 2) { has_goal2 = true; break; }
    }
    bool all_coins = (!gs || (gs->coins_collected_count >= lvl->num_coins));
    u32 clrGoalFull  = C2D_Color32(0xFF, 0xD0, 0x20, 0xE0); // gold
    u32 clrGoalBord  = C2D_Color32(0xFF, 0x80, 0x10, 0xE0); // orange border
    u32 clrGoalFill  = C2D_Color32(0xE8, 0xE8, 0x60, 0xC0); // pale yellow fill
    for (uint16_t i = 0; i < lvl->num_checks; i++) {
        const CheckRect* c = &lvl->checks[i];
        float cx = ox + c->x * scale, cy = oy + c->y * scale;
        float cw = c->w * scale,      ch = c->h * scale;
        bool is_goal = (c->kind == 2) || (!has_goal2 && c->kind == 1 && gs && gs->left_start_check);
        if (is_goal) {
            if (all_coins) {
                C2D_DrawRectSolid(cx, cy, 0.09f, cw, ch, clrGoalFull);
            } else {
                C2D_DrawRectSolid(cx, cy, 0.09f, cw, ch, clrGoalBord);
                C2D_DrawRectSolid(cx + 2.0f, cy + 2.0f, 0.10f, cw - 4.0f, ch - 4.0f, clrGoalFill);
            }
        } else {
            C2D_DrawRectSolid(cx, cy, 0.1f, cw, ch, clrChk);
        }
    }

    // Seamless dark outline around all walls (pass 1: expanded by 1.5px)
    float wall_ox = ox + depth_3d * 1.5f;
    for (uint16_t i = 0; i < lvl->num_walls; i++) {
        const Rect16* r = &lvl->walls[i];
        C2D_DrawRectSolid(wall_ox + r->x * scale - 1.5f, oy + r->y * scale - 1.5f, 0.18f,
                          r->w * scale + 3.0f, r->h * scale + 3.0f, clrWallOut);
    }
    // Wall fill (pass 2: exact rect in lavender)
    for (uint16_t i = 0; i < lvl->num_walls; i++) {
        const Rect16* r = &lvl->walls[i];
        C2D_DrawRectSolid(wall_ox + r->x * scale, oy + r->y * scale, 0.2f,
                          r->w * scale + 0.4f, r->h * scale + 0.4f, clrWall);
    }

    // Closed doors with dark outline
    for (uint16_t i = 0; i < lvl->num_doors; i++) {
        const DoorRect* d = &lvl->doors[i];
        if (!preview_only && gs && d->door_id < MAX_DOORS && gs->door_open[d->door_id]) continue;
        C2D_DrawRectSolid(wall_ox + d->x * scale - 1.0f, oy + d->y * scale - 1.0f, 0.23f,
                          d->w * scale + 2.0f, d->h * scale + 2.0f, clrWallOut);
        C2D_DrawRectSolid(wall_ox + d->x * scale, oy + d->y * scale, 0.25f,
                          d->w * scale, d->h * scale, clrDoor);
    }

    float ent_ox = ox + depth_3d * 3.0f;

    // 1. Player (rect - batched with walls/floor before switching to sprites/circles)
    if (!preview_only && gs) {
        u32 p_cols[6] = {
            C2D_Color32(0xFF, 0x18, 0x18, 0xFF), // Red
            C2D_Color32(0x20, 0xC8, 0x30, 0xFF), // Green
            C2D_Color32(0x28, 0x70, 0xFF, 0xFF), // Blue
            C2D_Color32(0xFF, 0xD0, 0x18, 0xFF), // Yellow
            C2D_Color32(0xD0, 0x30, 0xFF, 0xFF), // Purple
            C2D_Color32(0x20, 0xE0, 0xE8, 0xFF), // Cyan
        };
        int skin = player_skin;
        if (skin < 0 || skin > 5) skin = 0;
        u8 alpha = (u8)((gs->player_alpha * 255) / 100);
        u32 base = p_cols[skin];
        u32 fill = (base & 0x00FFFFFFu) | ((u32)alpha << 24);
        u32 outline = C2D_Color32(0x00, 0x00, 0x00, alpha);
        float px = ent_ox + gs->player_x * scale;
        float py = oy + gs->player_y * scale;
        float hs = PLAYER_HALF_SIZE * scale;
        C2D_DrawRectSolid(px - hs, py - hs, 0.6f, hs * 2.0f, hs * 2.0f, outline);
        C2D_DrawRectSolid(px - hs + 1.5f, py - hs + 1.5f, 0.62f,
                          hs * 2.0f - 3.0f, hs * 2.0f - 3.0f, fill);
    }

    // 2. Keys (textured sprites)
    for (uint16_t i = 0; i < lvl->num_keys; i++) {
        if (!preview_only && gs && gs->key_collected[i]) continue;
        draw_sprite_centered(sprites_key_idx,
                             ent_ox + lvl->keys[i].x * scale,
                             oy + lvl->keys[i].y * scale,
                             scale, 0.45f, 255);
    }

    // 3. Circles LAST (citro2d best practice: batch all circles together at the end)
    for (uint16_t i = 0; i < lvl->num_coins; i++) {
        if (!preview_only && gs && gs->coin_collected[i]) continue;
        float cx = ent_ox + lvl->coins[i].x * scale;
        float cy = oy + lvl->coins[i].y * scale;
        float r = 6.5f * scale;
        C2D_DrawCircleSolid(cx, cy, 0.4f, r + 1.0f, C2D_Color32(0x10, 0x10, 0x20, 0xFF));
        C2D_DrawCircleSolid(cx, cy, 0.41f, r, C2D_Color32(0xFF, 0xD8, 0x10, 0xFF));
        C2D_DrawCircleSolid(cx - 0.6f, cy - 0.6f, 0.42f, r * 0.45f, C2D_Color32(0xFF, 0xFA, 0xA0, 0xFF));
    }

    float anim_f = (gs && !preview_only) ? gs->enemy_frame : 0.0f;
    for (uint16_t i = 0; i < lvl->num_enemies; i++) {
        float ex = 0, ey = 0;
        level_get_enemy_pos(&lvl->enemies[i], anim_f, &ex, &ey);
        float cx = ent_ox + ex * scale;
        float cy = oy + ey * scale;
        float r = lvl->enemies[i].radius * scale;
        C2D_DrawCircleSolid(cx, cy, 0.5f, r + 1.0f, C2D_Color32(0x08, 0x08, 0x18, 0xFF));
        C2D_DrawCircleSolid(cx, cy, 0.51f, r, C2D_Color32(0x18, 0x38, 0xFF, 0xFF));
        C2D_DrawCircleSolid(cx - r * 0.25f, cy - r * 0.25f, 0.52f, r * 0.35f, C2D_Color32(0x88, 0xB8, 0xFF, 0xFF));
    }
    (void)ph;
}

void render_minimap(const LevelData* lvl, const GameState* gs,
                    float mx, float my, float mw, float mh) {
    if (!lvl) return;
    float sx = mw / (float)lvl->width;
    float sy = mh / (float)lvl->height;

    render_draw_card(mx - 2.0f, my - 2.0f, mw + 4.0f, mh + 4.0f,
                     C2D_Color32(0x12, 0x14, 0x24, 0xFF),
                     C2D_Color32(0x55, 0x60, 0x98, 0xFF));

    for (uint16_t i = 0; i < lvl->num_checks; i++) {
        const CheckRect* c = &lvl->checks[i];
        C2D_DrawRectSolid(mx + c->x * sx, my + c->y * sy, 0.15f,
                          c->w * sx, c->h * sy, C2D_Color32(0x45, 0xC8, 0x55, 0xFF));
    }
    for (uint16_t i = 0; i < lvl->num_walls; i++) {
        const Rect16* r = &lvl->walls[i];
        C2D_DrawRectSolid(mx + r->x * sx, my + r->y * sy, 0.2f,
                          r->w * sx, r->h * sy, C2D_Color32(0x78, 0x80, 0xC0, 0xFF));
    }
    if (gs) {
        for (uint16_t i = 0; i < lvl->num_coins; i++) {
            if (gs->coin_collected[i]) continue;
            C2D_DrawRectSolid(mx + lvl->coins[i].x * sx - 1.5f, my + lvl->coins[i].y * sy - 1.5f,
                              0.25f, 3.0f, 3.0f, C2D_Color32(0xFF, 0xE0, 0x20, 0xFF));
        }
        C2D_DrawRectSolid(mx + gs->player_x * sx - 2.0f, my + gs->player_y * sy - 2.0f,
                          0.3f, 4.0f, 4.0f, C2D_Color32(0xFF, 0x35, 0x35, 0xFF));
    }
}
