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
    C2D_DrawRectSolid(x + 2.0f, y + 3.0f, 0.0f, w, h, C2D_Color32(0, 0, 0, 110));
    C2D_DrawRectSolid(x, y, 0.0f, w, h, border_col);
    C2D_DrawRectSolid(x + 1.5f, y + 1.5f, 0.0f, w - 3.0f, h - 3.0f, bg_col);
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
    C2D_DrawRectSolid(x + 2.0f, y + 3.0f, 0.1f, w, h, C2D_Color32(0, 0, 0, 120));
    C2D_DrawRectSolid(x, y, 0.15f, w, h, border_color);
    C2D_DrawRectangle(x + 1.5f, y + 1.5f, 0.2f, w - 3.0f, h - 3.0f,
                      bg_top, bg_top, bg_bot, bg_bot);
    C2D_DrawRectSolid(x + 2.5f, y + 2.0f, 0.25f, w - 5.0f, 1.5f, C2D_Color32(255, 255, 255, 55));
    if (btn->label) {
        render_draw_text_centered_shadow(x + w * 0.5f, y + h * 0.5f - 8.5f,
                                         0.58f, text_color, btn->label);
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
    for (int i = 0; i < 8; i++) {
        float ang = time_sec * 1.4f + i * 0.7854f;
        float cx = 200.0f + cosf(ang) * 145.0f + depth_3d * 1.5f;
        float cy = 122.0f + sinf(ang * 1.3f) * 78.0f;
        draw_sprite_centered(sprites_enemy_blue_idx, cx, cy, 0.45f, 0.1f, 255);
    }
    float px = 200.0f + sinf(time_sec * 1.9f) * 65.0f + depth_3d * 2.5f;
    float py = 122.0f + cosf(time_sec * 2.2f) * 28.0f;
    C2D_DrawRectSolid(px - 10.0f, py - 10.0f, 0.2f, 20.0f, 20.0f, C2D_Color32(0, 0, 0, 255));
    C2D_DrawRectSolid(px - 7.0f, py - 7.0f, 0.25f, 14.0f, 14.0f, C2D_Color32(255, 30, 30, 255));
}

static u32 terrain_color(uint8_t t) {
    switch (t) {
        case TERR_WATER: return C2D_Color32(45, 130, 235, 190);
        case TERR_ICE:   return C2D_Color32(185, 240, 255, 220);
        case TERR_PIT:   return C2D_Color32(18, 18, 24, 255);
        case TERR_CBR1: case TERR_CBL1: case TERR_CBU1: case TERR_CBD1:
            return C2D_Color32(120, 125, 140, 220);
        case TERR_CBR2: case TERR_CBL2: case TERR_CBU2: case TERR_CBD2:
            return C2D_Color32(95, 145, 180, 220);
        case TERR_CBR3: case TERR_CBL3: case TERR_CBU3: case TERR_CBD3:
            return C2D_Color32(180, 95, 110, 220);
        default: return C2D_Color32(200, 200, 200, 255);
    }
}

void render_level_top(const LevelData* lvl, const GameState* gs,
                      int player_skin, float depth_3d, float zoom, bool preview_only) {
    if (!lvl) return;

    float sx = (float)TOP_W / (float)lvl->width;
    float sy = (float)TOP_H / (float)lvl->height;
    float base_s = (sx < sy) ? sx : sy;
    if (zoom < 1.0f) zoom = 1.0f;
    float s = base_s * zoom;

    float ox, oy;
    if (zoom <= 1.01f || !gs || preview_only) {
        ox = (TOP_W - lvl->width * s) * 0.5f;
        oy = (TOP_H - lvl->height * s) * 0.5f;
    } else {
        ox = TOP_W * 0.5f - gs->player_x * s;
        oy = TOP_H * 0.5f - gs->player_y * s;
        float min_ox = TOP_W - lvl->width * s;
        float min_oy = TOP_H - lvl->height * s;
        if (ox > 0.0f) ox = 0.0f;
        if (ox < min_ox) ox = min_ox;
        if (oy > 0.0f) oy = 0.0f;
        if (oy < min_oy) oy = min_oy;
    }

    // Background void
    C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, (float)TOP_W, (float)TOP_H, C2D_Color32(22, 22, 32, 255));
    C2D_DrawRectSolid(ox, oy, 0.0f, lvl->width * s, lvl->height * s, C2D_Color32(236, 236, 246, 255));

    // Checkerboard floor
    float tile = 25.0f * s;
    int cols = lvl->width / 25;
    int rows = lvl->height / 25;
    u32 chk_col = C2D_Color32(216, 216, 234, 255);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if ((r + c) & 1) {
                C2D_DrawRectSolid(ox + c * tile, oy + r * tile, 0.02f, tile, tile, chk_col);
            }
        }
    }

    // Terrains
    for (uint16_t i = 0; i < lvl->num_terrains; i++) {
        const TerrainRect* t = &lvl->terrains[i];
        float tx = ox + t->x * s, ty = oy + t->y * s, tw = t->w * s, th = t->h * s;
        C2D_DrawRectSolid(tx, ty, 0.04f, tw, th, terrain_color(t->type));
        // Conveyor direction stripe
        if (t->type >= TERR_CBR1 && t->type <= TERR_CBD3) {
            u32 stripe = C2D_Color32(255, 255, 255, 90);
            if (t->type <= TERR_CBL3) {
                C2D_DrawRectSolid(tx, ty + th * 0.4f, 0.045f, tw, th * 0.2f, stripe);
            } else {
                C2D_DrawRectSolid(tx + tw * 0.4f, ty, 0.045f, tw * 0.2f, th, stripe);
            }
        }
    }

    // Moving platforms
    float anim = gs ? gs->enemy_frame : 0.0f;
    for (uint16_t i = 0; i < lvl->num_mps; i++) {
        float mx, my;
        level_get_mp_pos(&lvl->mps[i], anim, &mx, &my);
        float rx = ox + mx * s, ry = oy + my * s;
        float rw = lvl->mps[i].w * s, rh = lvl->mps[i].h * s;
        C2D_DrawRectSolid(rx, ry, 0.05f, rw, rh, C2D_Color32(45, 45, 55, 255));
        C2D_DrawRectSolid(rx + 1.5f, ry + 1.5f, 0.055f, rw - 3.0f, rh - 3.0f, C2D_Color32(150, 115, 85, 255));
    }

    // Checkpoints and Goal
    // Goal pad = gold when all cash collected, orange border when cash missing; checkpoints = green
    const Whg4StartFinish* fin = gs ? &WHG4_FINISHES[gs->cur_level - 1] : NULL;
    bool is_finish_room4 = fin && gs && (gs->cur_col == fin->col && gs->cur_row == fin->row);
    // WHG4 cash requirement for goal (total coins + diamonds*100)
    int total_cash4 = 0;
    if (gs) {
        for (uint16_t ci = 0; ci < lvl->num_coins && ci < MAX_ROOM_COINS; ci++) {
            if (!gs->coin_collected || !gs->coin_collected[ci]) total_cash4 += 1;
        }
        // cash = already-collected across level, check gs fields
        // Use simpler approach: pad is gold when player has collected all coins in current room
        // Actually compare to WHG4_LEVEL_CASH via extern function:
        total_cash4 = 0; // reset – will use coin_collected count approximation below
    }
    // Simple approach: count uncollected coins in this room; gold when 0 uncollected remain
    int uncollected_coins4 = 0;
    if (gs && gs->coin_collected) {
        for (uint16_t ci = 0; ci < lvl->num_coins && ci < MAX_ROOM_COINS; ci++) {
            if (!gs->coin_collected[ci]) uncollected_coins4++;
        }
    }
    int uncollected_dias4 = 0;
    if (gs && gs->diamond_collected) {
        for (uint16_t ci = 0; ci < lvl->num_diamonds && ci < MAX_ROOM_DIAMONDS; ci++) {
            if (!gs->diamond_collected[ci]) uncollected_dias4++;
        }
    }
    bool all_cash4 = (uncollected_coins4 == 0 && uncollected_dias4 == 0);
    u32 clrGoalFull4 = C2D_Color32(0xFF, 0xD0, 0x20, 0xE0);
    u32 clrGoalBord4 = C2D_Color32(0xFF, 0x80, 0x10, 0xE0);
    u32 clrGoalFill4 = C2D_Color32(0xE8, 0xE8, 0x60, 0xC0);
    u32 clrChk4      = C2D_Color32(152, 238, 152, 200);
    for (uint16_t i = 0; i < lvl->num_checks; i++) {
        const CheckRect* c = &lvl->checks[i];
        float cx4 = ox + c->x * s, cy4 = oy + c->y * s;
        float cw4 = c->w * s,      ch4 = c->h * s;
        bool is_goal = is_finish_room4 && fin && (c->kind == fin->check_idx);
        if (is_goal) {
            if (all_cash4) {
                C2D_DrawRectSolid(cx4, cy4, 0.06f, cw4, ch4, clrGoalFull4);
            } else {
                C2D_DrawRectSolid(cx4, cy4, 0.06f, cw4, ch4, clrGoalBord4);
                C2D_DrawRectSolid(cx4 + 2.0f, cy4 + 2.0f, 0.065f, cw4 - 4.0f, ch4 - 4.0f, clrGoalFill4);
            }
        } else {
            C2D_DrawRectSolid(cx4, cy4, 0.06f, cw4, ch4, clrChk4);
        }
    }

    // Walls
    u32 wall_col = C2D_Color32(155, 148, 225, 255);
    u32 wall_border = C2D_Color32(20, 20, 40, 255);
    for (uint16_t i = 0; i < lvl->num_walls; i++) {
        const Rect16* r = &lvl->walls[i];
        float rx = ox + r->x * s + depth_3d * 0.4f;
        float ry = oy + r->y * s;
        float rw = r->w * s;
        float rh = r->h * s;
        C2D_DrawRectSolid(rx, ry, 0.1f, rw, rh, wall_border);
        if (rw > 2.0f && rh > 2.0f) {
            C2D_DrawRectSolid(rx + 1.0f, ry + 1.0f, 0.12f, rw - 2.0f, rh - 2.0f, wall_col);
        }
    }

    // Doors
    for (uint16_t i = 0; i < lvl->num_doors; i++) {
        const DoorRect* d = &lvl->doors[i];
        bool is_gold = (d->door_id == 255);
        if (gs) {
            if (is_gold && gs->gold_door_open) continue;
            if (!is_gold && d->door_id < MAX_DOORS && gs->door_open[d->door_id]) continue;
        }
        u32 dcol = is_gold ? C2D_Color32(230, 175, 35, 255) : C2D_Color32(130, 130, 145, 255);
        C2D_DrawRectSolid(ox + d->x * s, oy + d->y * s, 0.15f, d->w * s, d->h * s, dcol);
    }

    // Coins
    float coin_scale = (COIN_RADIUS * 2.0f * s) / 16.0f;
    if (coin_scale < 0.25f) coin_scale = 0.25f;
    for (uint16_t i = 0; i < lvl->num_coins && i < MAX_ROOM_COINS; i++) {
        if (gs && gs->coin_collected && gs->coin_collected[i]) continue;
        draw_sprite_centered(sprites_coin_idx, ox + lvl->coins[i].x * s + depth_3d * 0.7f,
                             oy + lvl->coins[i].y * s, coin_scale, 0.2f, 255);
    }

    // Diamonds ($100)
    float dia_half = DIAMOND_RADIUS * s;
    if (dia_half < 3.0f) dia_half = 3.0f;
    for (uint16_t i = 0; i < lvl->num_diamonds && i < MAX_ROOM_DIAMONDS; i++) {
        if (gs && gs->diamond_collected && gs->diamond_collected[i]) continue;
        float dx = ox + lvl->diamonds[i].x * s + depth_3d * 0.7f;
        float dy = oy + lvl->diamonds[i].y * s;
        C2D_DrawRectSolid(dx - dia_half, dy - dia_half, 0.22f, dia_half * 2.0f, dia_half * 2.0f, C2D_Color32(20, 70, 120, 255));
        C2D_DrawRectSolid(dx - dia_half * 0.7f, dy - dia_half * 0.7f, 0.23f, dia_half * 1.4f, dia_half * 1.4f, C2D_Color32(80, 235, 255, 255));
    }

    // Keys
    float key_scale = (KEY_HALF_SIZE * 2.0f * s) / 22.0f;
    if (key_scale < 0.25f) key_scale = 0.25f;
    for (uint16_t i = 0; i < lvl->num_keys; i++) {
        if (gs && lvl->keys[i].door_id < MAX_DOORS && gs->key_collected[lvl->keys[i].door_id]) continue;
        draw_sprite_centered(sprites_key_idx, ox + lvl->keys[i].x * s + depth_3d * 0.7f,
                             oy + lvl->keys[i].y * s, key_scale, 0.25f, 255);
    }

    // Enemies
    for (uint16_t i = 0; i < lvl->num_enemies; i++) {
        float ex, ey;
        level_get_enemy_pos(&lvl->enemies[i], anim, &ex, &ey);
        float es = (lvl->enemies[i].radius * 2.0f * s) / 22.0f;
        if (es < 0.25f) es = 0.25f;
        draw_sprite_centered(sprites_enemy_blue_idx, ox + ex * s + depth_3d * 1.0f,
                             oy + ey * s, es, 0.35f, 255);
    }

    // Player
    if (!preview_only && gs) {
        u8 alpha = (u8)((gs->player_alpha * 255) / 100);
        float ps = PLAYER_HALF_SIZE * 2.0f * s;
        float px = ox + gs->player_x * s - ps * 0.5f + depth_3d * 1.2f;
        float py = oy + gs->player_y * s - ps * 0.5f;
        static const u32 skin_cols[6] = {
            0xFF1E1EFF, 0xFFFF7828, 0xFF3CC83C, 0xFF28D2FF, 0xFFF046D2, 0xFF28A0FF
        };
        u32 fill = skin_cols[(player_skin >= 0 && player_skin < 6) ? player_skin : 0];
        fill = (fill & 0x00FFFFFFu) | ((u32)alpha << 24);
        u32 border = ((u32)alpha << 24);
        C2D_DrawRectSolid(px, py, 0.4f, ps, ps, border);
        float inset = 2.0f * s;
        if (inset < 1.0f) inset = 1.0f;
        if (ps > inset * 2.0f) {
            C2D_DrawRectSolid(px + inset, py + inset, 0.42f, ps - inset * 2.0f, ps - inset * 2.0f, fill);
        }
        // Water oxygen bar
        if (gs->in_water) {
            float ratio = (float)gs->water_ticks / (float)WATER_MAX_TICKS;
            float bw = ps * 1.2f;
            float bx = px + ps * 0.5f - bw * 0.5f;
            float by = py - 6.0f;
            C2D_DrawRectSolid(bx, by, 0.45f, bw, 3.5f, C2D_Color32(0, 0, 0, 200));
            C2D_DrawRectSolid(bx + 0.5f, by + 0.5f, 0.46f, (bw - 1.0f) * ratio, 2.5f, C2D_Color32(60, 210, 255, 255));
        }
    }
}

void render_minimap(const LevelData* lvl, const GameState* gs,
                    float mx, float my, float mw, float mh) {
    if (!lvl) return;
    render_draw_card(mx - 2.0f, my - 2.0f, mw + 4.0f, mh + 4.0f,
                     C2D_Color32(28, 30, 50, 240), C2D_Color32(85, 95, 145, 255));
    float s = mw / (float)lvl->width;
    float sy = mh / (float)lvl->height;
    if (sy < s) s = sy;
    float ox = mx + (mw - lvl->width * s) * 0.5f;
    float oy = my + (mh - lvl->height * s) * 0.5f;

    C2D_DrawRectSolid(ox, oy, 0.05f, lvl->width * s, lvl->height * s, C2D_Color32(225, 225, 240, 255));
    for (uint16_t i = 0; i < lvl->num_terrains; i++) {
        const TerrainRect* t = &lvl->terrains[i];
        C2D_DrawRectSolid(ox + t->x * s, oy + t->y * s, 0.06f, t->w * s, t->h * s, terrain_color(t->type));
    }
    for (uint16_t i = 0; i < lvl->num_walls; i++) {
        const Rect16* r = &lvl->walls[i];
        C2D_DrawRectSolid(ox + r->x * s, oy + r->y * s, 0.1f, r->w * s, r->h * s, C2D_Color32(105, 100, 175, 255));
    }
    if (gs) {
        for (uint16_t i = 0; i < lvl->num_coins && i < MAX_ROOM_COINS; i++) {
            if (gs->coin_collected && gs->coin_collected[i]) continue;
            C2D_DrawRectSolid(ox + lvl->coins[i].x * s - 1.0f, oy + lvl->coins[i].y * s - 1.0f,
                              0.2f, 2.0f, 2.0f, C2D_Color32(255, 210, 0, 255));
        }
        for (uint16_t i = 0; i < lvl->num_diamonds && i < MAX_ROOM_DIAMONDS; i++) {
            if (gs->diamond_collected && gs->diamond_collected[i]) continue;
            C2D_DrawRectSolid(ox + lvl->diamonds[i].x * s - 1.5f, oy + lvl->diamonds[i].y * s - 1.5f,
                              0.22f, 3.0f, 3.0f, C2D_Color32(80, 235, 255, 255));
        }
        float px = ox + gs->player_x * s - 2.0f;
        float py = oy + gs->player_y * s - 2.0f;
        C2D_DrawRectSolid(px, py, 0.3f, 4.0f, 4.0f, C2D_Color32(255, 40, 40, 255));
    }
}
