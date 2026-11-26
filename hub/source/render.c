#include "render.h"
#include "sprites.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static C2D_SpriteSheet s_sheet = NULL;
static C2D_TextBuf s_text_buf = NULL;
static C2D_Font s_font = NULL;

/* ---- texte : cache des chaines deja analysees ----
 * Avant : chaque dessin ET chaque mesure de largeur re-analysait la chaine (C2D_TextFontParse +
 * Optimize), plusieurs centaines de fois par image dans les menus et le HUD (ex. la boucle de
 * reduction de ui_stat_tile). Maintenant une chaine n'est analysee qu'une seule fois. */
#define TC_SLOTS  256
#define TC_MAXLEN 64
#define TC_GLYPHS 6144

typedef struct {
    bool used;
    char str[TC_MAXLEN];
    C2D_Text text;
    float w1, h1;          /* dimensions a l'echelle 1.0 (la largeur est lineaire en l'echelle) */
} TextEntry;

typedef struct {
    const C2D_Text* text;
    float w1, h1;
} TextRef;

static TextEntry s_tc[TC_SLOTS];
static C2D_TextBuf s_cache_buf = NULL;

bool render_init(void) {
    s_sheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
    if (!s_sheet) return false;
    /* Les sprites du jeu sont du pixel art : garder les pixels nets pendant le zoom. */
    C2D_Image first_sprite = C2D_SpriteSheetGetImage(s_sheet, 0);
    if (first_sprite.tex) C3D_TexSetFilter(first_sprite.tex, GPU_NEAREST, GPU_NEAREST);
    s_text_buf = C2D_TextBufNew(12288);
    s_cache_buf = C2D_TextBufNew(TC_GLYPHS);
    s_font = C2D_FontLoad("romfs:/font.bcfnt");
    if (!s_font) {
        s_font = C2D_FontLoadSystem(CFG_REGION_EUR);
    }
    return true;
}

void render_exit(void) {
    if (s_font) { C2D_FontFree(s_font); s_font = NULL; }
    if (s_text_buf) { C2D_TextBufDelete(s_text_buf); s_text_buf = NULL; }
    if (s_cache_buf) { C2D_TextBufDelete(s_cache_buf); s_cache_buf = NULL; }
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

static uint32_t text_hash(const char* str) {
    uint32_t hv = 2166136261u;
    while (*str) { hv ^= (uint8_t)*str++; hv *= 16777619u; }
    return hv;
}

static void parse_into(C2D_Text* t, C2D_TextBuf buf, const char* text) {
    if (s_font) {
        C2D_TextFontParse(t, s_font, buf, text);
    } else {
        C2D_TextParse(t, buf, text);
    }
    C2D_TextOptimize(t);
}

static void text_cache_flush(void) {
    if (s_cache_buf) C2D_TextBufClear(s_cache_buf);
    memset(s_tc, 0, sizeof(s_tc));
}

/* Renvoie le texte analyse (depuis le cache si possible). Une analyse n'est utilisee que pendant
 * l'appel de dessin, donc vider le cache en cours d'image est sans danger. */
static bool text_obtain(const char* text, TextRef* ref, C2D_Text* scratch) {
    size_t len = strlen(text);
    if (s_cache_buf && len < TC_MAXLEN) {
        uint32_t hv = text_hash(text);
        for (int attempt = 0; attempt < 2; attempt++) {
            TextEntry* slot = NULL;
            for (int p = 0; p < 8; p++) {
                TextEntry* e = &s_tc[(hv + (uint32_t)p) & (TC_SLOTS - 1)];
                if (!e->used) { slot = e; break; }
                if (strcmp(e->str, text) == 0) {
                    ref->text = &e->text; ref->w1 = e->w1; ref->h1 = e->h1;
                    return true;
                }
            }
            if (slot && C2D_TextBufGetNumGlyphs(s_cache_buf) + len <= TC_GLYPHS) {
                parse_into(&slot->text, s_cache_buf, text);
                C2D_TextGetDimensions(&slot->text, 1.0f, 1.0f, &slot->w1, &slot->h1);
                memcpy(slot->str, text, len + 1);
                slot->used = true;
                ref->text = &slot->text; ref->w1 = slot->w1; ref->h1 = slot->h1;
                return true;
            }
            text_cache_flush();   /* table ou tampon plein : on repart de zero (une fois) */
        }
    }
    /* chaine trop longue : analyse jetable dans le tampon de l'image */
    if (!s_text_buf) return false;
    parse_into(scratch, s_text_buf, text);
    C2D_TextGetDimensions(scratch, 1.0f, 1.0f, &ref->w1, &ref->h1);
    ref->text = scratch;
    return true;
}

void render_draw_text(float x, float y, float scale, u32 color, const char* text) {
    render_draw_text_depth(x, y, scale, color, 0.5f, text);
}

void render_draw_text_depth(float x, float y, float scale, u32 color, float depth, const char* text) {
    if (!text) return;
    TextRef r; C2D_Text scratch;
    if (!text_obtain(text, &r, &scratch)) return;
    C2D_DrawText(r.text, C2D_WithColor, x, y, depth, scale, scale, color);
}

void render_draw_text_shadow(float x, float y, float scale, u32 color, const char* text) {
    if (!text) return;
    TextRef r; C2D_Text scratch;
    if (!text_obtain(text, &r, &scratch)) return;
    C2D_DrawText(r.text, C2D_WithColor, x + 1.5f, y + 1.5f, 0.45f, scale, scale, C2D_Color32(0, 0, 0, 180));
    C2D_DrawText(r.text, C2D_WithColor, x, y, 0.5f, scale, scale, color);
}

void render_draw_text_centered(float cx, float y, float scale, u32 color, const char* text) {
    render_draw_text_centered_depth(cx, y, scale, color, 0.5f, text);
}

void render_draw_text_centered_shadow(float cx, float y, float scale, u32 color, const char* text) {
    if (!text) return;
    TextRef r; C2D_Text scratch;
    if (!text_obtain(text, &r, &scratch)) return;
    float x = cx - r.w1 * scale * 0.5f;
    C2D_DrawText(r.text, C2D_WithColor, x + 1.5f, y + 1.5f, 0.45f, scale, scale, C2D_Color32(0, 0, 0, 190));
    C2D_DrawText(r.text, C2D_WithColor, x, y, 0.5f, scale, scale, color);
}

void render_draw_text_centered_depth(float cx, float y, float scale, u32 color, float depth, const char* text) {
    if (!text) return;
    TextRef r; C2D_Text scratch;
    if (!text_obtain(text, &r, &scratch)) return;
    C2D_DrawText(r.text, C2D_WithColor, cx - r.w1 * scale * 0.5f, y, depth, scale, scale, color);
}

float render_text_width(float scale, const char* text) {
    if (!text || !text[0]) return 0.0f;
    TextRef r; C2D_Text scratch;
    if (!text_obtain(text, &r, &scratch)) return 0.0f;
    return r.w1 * scale;
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
        float phase = time_sec * (0.7f + (i % 3) * 0.25f) + i * 1.1f;
        float x = 35.0f + i * 48.0f + depth_3d * 2.0f;
        float y = 120.0f + sinf(phase) * 75.0f;
        draw_sprite_centered((i % 2 == 0) ? sprites_enemy_blue_idx : sprites_coin_idx,
                             x, y, 1.0f, 0.15f, 110);
    }
}

u32 render_skin_color(int skin) {
    const u32 cols[6] = {   /* pas 'static' : C2D_Color32 est une fonction, pas une constante */
        C2D_Color32(0xFF, 0x18, 0x18, 0xFF),
        C2D_Color32(0x20, 0xC8, 0x30, 0xFF),
        C2D_Color32(0x28, 0x70, 0xFF, 0xFF),
        C2D_Color32(0xFF, 0xD0, 0x18, 0xFF),
        C2D_Color32(0xD0, 0x30, 0xFF, 0xFF),
        C2D_Color32(0x20, 0xE0, 0xE8, 0xFF),
    };
    return cols[(skin >= 0 && skin <= 5) ? skin : 0];
}

/* ---- camera : suivi du joueur adouci (plus de saut brusque en zoom / changement de salle) ---- */
static float s_cam_ox = 0.0f, s_cam_oy = 0.0f, s_cam_zoom = 0.0f;
static bool s_cam_valid = false;

static void compute_view(const LevelData* lvl, const GameState* gs, float zoom, bool preview_only,
                         float* out_scale, float* out_ox, float* out_oy, float* out_pw, float* out_ph) {
    float sx = (float)TOP_W / (float)lvl->width;
    float sy = (float)TOP_H / (float)lvl->height;
    float base_scale = (sx < sy) ? sx : sy;
    float scale = base_scale * zoom;
    float pw = lvl->width * scale;
    float ph = lvl->height * scale;
    float ox = (TOP_W - pw) * 0.5f;
    float oy = (TOP_H - ph) * 0.5f;

    if (zoom > 1.01f && gs && !preview_only) {
        if (pw > TOP_W) {
            ox = TOP_W * 0.5f - gs->player_x * scale;
            if (ox > 0.0f) ox = 0.0f;
            if (ox < TOP_W - pw) ox = TOP_W - pw;
        }
        if (ph > TOP_H) {
            oy = TOP_H * 0.5f - gs->player_y * scale;
            if (oy > 0.0f) oy = 0.0f;
            if (oy < TOP_H - ph) oy = TOP_H - ph;
        }
    }
    *out_scale = scale; *out_ox = ox; *out_oy = oy; *out_pw = pw; *out_ph = ph;
}

void render_camera_reset(void) {
    s_cam_valid = false;
}

void render_camera_update(const LevelData* lvl, const GameState* gs, float zoom) {
    if (!lvl || !gs || lvl->width == 0 || lvl->height == 0 || zoom <= 1.01f) {
        s_cam_valid = false;
        return;
    }
    float scale, ox, oy, pw, ph;
    compute_view(lvl, gs, zoom, false, &scale, &ox, &oy, &pw, &ph);
    if (!s_cam_valid || s_cam_zoom != zoom ||
        fabsf(ox - s_cam_ox) > TOP_W * 0.5f || fabsf(oy - s_cam_oy) > TOP_H * 0.5f) {
        s_cam_ox = ox; s_cam_oy = oy; s_cam_zoom = zoom; s_cam_valid = true;   /* snap */
    } else {
        s_cam_ox += (ox - s_cam_ox) * 0.22f;
        s_cam_oy += (oy - s_cam_oy) * 0.22f;
    }
}

/* ---- effets : particules et tremblement d'ecran ----
 * Pool fixe (aucune allocation). Les particules vivent en coordonnees du monde, donc elles
 * suivent la camera et le zoom. Mise a jour par pas de simulation (60 Hz), pas par image. */
#define MAX_PARTICLES 72

typedef struct {
    float x, y, vx, vy, size;
    uint8_t life, max_life;
    uint8_t r, g, b;
} Particle;

static Particle s_parts[MAX_PARTICLES];
static bool s_fx_on = true;
static float s_shake_amt = 0.0f, s_shake_dx = 0.0f, s_shake_dy = 0.0f;
static uint32_t s_rng = 0x2F6E2B1u;

static float frand(void) {
    s_rng = s_rng * 1664525u + 1013904223u;
    return (float)((s_rng >> 8) & 0xFFFFu) / 65535.0f;
}

void render_fx_clear(void) {
    memset(s_parts, 0, sizeof(s_parts));
    s_shake_amt = s_shake_dx = s_shake_dy = 0.0f;
}

void render_fx_enable(bool on) {
    s_fx_on = on;
    if (!on) render_fx_clear();
}

void render_fx_burst(float world_x, float world_y, u32 color, int count, float speed, float size) {
    if (!s_fx_on) return;
    for (int n = 0; n < count; n++) {
        Particle* p = NULL;
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (s_parts[i].life == 0) { p = &s_parts[i]; break; }
        }
        if (!p) return;
        float ang = frand() * 6.2831853f;
        float sp = speed * (0.35f + 0.65f * frand());
        p->x = world_x; p->y = world_y;
        p->vx = cosf(ang) * sp; p->vy = sinf(ang) * sp;
        p->size = size * (0.6f + 0.6f * frand());
        p->max_life = (uint8_t)(16 + (int)(frand() * 16.0f));
        p->life = p->max_life;
        p->r = (uint8_t)(color & 0xFF);
        p->g = (uint8_t)((color >> 8) & 0xFF);
        p->b = (uint8_t)((color >> 16) & 0xFF);
    }
}

void render_fx_shake(float amount) {
    if (!s_fx_on) return;
    if (amount > 4.0f) amount = 4.0f;
    if (amount > s_shake_amt) s_shake_amt = amount;
}

void render_fx_update(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &s_parts[i];
        if (p->life == 0) continue;
        p->x += p->vx; p->y += p->vy;
        p->vx *= 0.92f; p->vy *= 0.92f;
        p->life--;
    }
    if (s_shake_amt > 0.05f) {
        /* un seul decalage par pas, partage par les deux yeux de la 3D */
        s_shake_dx = (frand() - 0.5f) * 2.0f * s_shake_amt;
        s_shake_dy = (frand() - 0.5f) * 2.0f * s_shake_amt;
        s_shake_amt *= 0.84f;
    } else {
        s_shake_amt = s_shake_dx = s_shake_dy = 0.0f;
    }
}

static void draw_particles(float ent_ox, float oy, float scale) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle* p = &s_parts[i];
        if (p->life == 0) continue;
        float k = (float)p->life / (float)p->max_life;
        float sz = p->size * scale * (0.45f + 0.55f * k);
        if (sz < 1.0f) sz = 1.0f;
        float px = ent_ox + p->x * scale, py = oy + p->y * scale;
        if (px < -8.0f || px > TOP_W + 8.0f || py < -8.0f || py > TOP_H + 8.0f) continue;
        C2D_DrawRectSolid(px - sz, py - sz, 0.7f, sz * 2.0f, sz * 2.0f,
                          C2D_Color32(p->r, p->g, p->b, (u8)(k * 255.0f)));
    }
}

static u32 terrain_color_hub(uint8_t t) {
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
    if (!lvl || lvl->width == 0 || lvl->height == 0) return;

    if (zoom < 1.0f) zoom = 1.0f;
    float scale, ox, oy, pw, ph;
    compute_view(lvl, gs, zoom, preview_only, &scale, &ox, &oy, &pw, &ph);
    if (zoom > 1.01f && gs && !preview_only && s_cam_valid && s_cam_zoom == zoom) {
        ox = s_cam_ox;
        oy = s_cam_oy;
        if (pw > TOP_W) {
            if (ox > 0.0f) ox = 0.0f;
            if (ox < TOP_W - pw) ox = TOP_W - pw;
        }
        if (ph > TOP_H) {
            if (oy > 0.0f) oy = 0.0f;
            if (oy < TOP_H - ph) oy = TOP_H - ph;
        }
        ox = roundf(ox);   /* pas de scintillement des traits fins */
        oy = roundf(oy);
    }

    if (!preview_only && gs && s_fx_on) { ox += s_shake_dx; oy += s_shake_dy; }   /* tremblement a la mort */

    C2D_DrawRectSolid(ox - 3.0f, 0.0f, 0.0f, pw + 6.0f, TOP_H, C2D_Color32(0x28, 0x2C, 0x52, 0xFF));
    C2D_DrawRectSolid(ox - 1.5f, 0.0f, 0.0f, pw + 3.0f, TOP_H, C2D_Color32(0x10, 0x10, 0x20, 0xFF));

    u32 clrBg1     = C2D_Color32(0xF8, 0xF8, 0xFF, 0xFF);
    u32 clrBg2     = C2D_Color32(0xE2, 0xE2, 0xFA, 0xFF);
    u32 clrWallOut = C2D_Color32(0x18, 0x18, 0x38, 0xFF);
    u32 clrWall    = C2D_Color32(0xB4, 0xB4, 0xFF, 0xFF);
    u32 clrDoor    = C2D_Color32(0xFF, 0xA8, 0x20, 0xFF);
    u32 clrChk     = C2D_Color32(0x88, 0xF4, 0x88, 0xD0);

    C2D_DrawRectSolid(ox, oy, 0.04f, pw, ph, clrBg1);
    const int tile = 25;
    float tile_px = tile * scale;
    int c_start = (int)((0.0f - ox) / tile_px); if (c_start < 0) c_start = 0;
    int c_end   = (int)((TOP_W - ox) / tile_px) + 1; if (c_end > lvl->width / tile) c_end = lvl->width / tile;
    int r_start = (int)((0.0f - oy) / tile_px); if (r_start < 0) r_start = 0;
    int r_end   = (int)((TOP_H - oy) / tile_px) + 1; if (r_end > lvl->height / tile) r_end = lvl->height / tile;
    if (tile_px < 5.0f) { r_end = r_start; }   /* cases invisibles : on economise des centaines de rectangles */
    for (int r = r_start; r < r_end; r++) {
        for (int c = c_start; c < c_end; c++) {
            if (((c + r) & 1) != 0) {
                C2D_DrawRectSolid(ox + c * tile_px, oy + r * tile_px, 0.05f,
                                  tile_px + 0.5f, tile_px + 0.5f, clrBg2);
            }
        }
    }

    for (uint16_t i = 0; i < lvl->num_terrains; i++) {
        const TerrainRect* t = &lvl->terrains[i];
        float tx = ox + t->x * scale, ty = oy + t->y * scale, tw = t->w * scale, th = t->h * scale;
        if (tx + tw < 0.0f || tx > TOP_W || ty + th < 0.0f || ty > TOP_H) continue;
        C2D_DrawRectSolid(tx, ty, 0.06f, tw, th, terrain_color_hub(t->type));
        if (t->type >= TERR_CBR1 && t->type <= TERR_CBD3) {
            u32 stripe = C2D_Color32(255, 255, 255, 90);
            if (t->type <= TERR_CBL3) C2D_DrawRectSolid(tx, ty + th * 0.4f, 0.065f, tw, th * 0.2f, stripe);
            else C2D_DrawRectSolid(tx + tw * 0.4f, ty, 0.065f, tw * 0.2f, th, stripe);
        }
    }

    float anim_mp = (gs && !preview_only) ? gs->enemy_frame : 0.0f;
    for (uint16_t i = 0; i < lvl->num_mps; i++) {
        float mx, my;
        level_get_mp_pos(&lvl->mps[i], anim_mp, &mx, &my);
        float rx = ox + mx * scale, ry = oy + my * scale;
        float rw = lvl->mps[i].w * scale, rh = lvl->mps[i].h * scale;
        C2D_DrawRectSolid(rx, ry, 0.08f, rw, rh, C2D_Color32(45, 45, 55, 255));
        if (rw > 3.0f && rh > 3.0f) {
            C2D_DrawRectSolid(rx + 1.5f, ry + 1.5f, 0.085f, rw - 3.0f, rh - 3.0f, C2D_Color32(150, 115, 85, 255));
        }
    }

    bool has_goal_chk = false;
    bool all_coins_got = false;
    if (gs && !preview_only) {
        if (gs->is_whg4) {
            all_coins_got = (game_total_cash(gs) >= WHG4_LEVEL_CASH[gs->cur_level - 1]);
        } else if (gs->is_whg3) {
            all_coins_got = (gs->coins_collected_count >= WHG3_LEVEL_COINS[gs->cur_level - 1]);
        } else {
            all_coins_got = (gs->coins_collected_count >= lvl->num_coins);
            for (uint16_t i = 0; i < lvl->num_checks; i++) {
                if (lvl->checks[i].kind == 2) { has_goal_chk = true; break; }
            }
        }
    } else {
        for (uint16_t i = 0; i < lvl->num_checks; i++) {
            if (lvl->checks[i].kind == 2) { has_goal_chk = true; break; }
        }
    }

    for (uint16_t i = 0; i < lvl->num_checks; i++) {
        const CheckRect* c = &lvl->checks[i];
        bool is_goal = false;
        if (gs && gs->is_whg3) {
            const Whg3StartFinish* fin = &WHG3_FINISHES[gs->cur_level - 1];
            is_goal = (gs->cur_col == fin->col && gs->cur_row == fin->row && c->kind == fin->check_idx);
        } else if (gs && gs->is_whg4) {
            const Whg4StartFinish* fin = &WHG4_FINISHES[gs->cur_level - 1];
            is_goal = (gs->cur_col == fin->col && gs->cur_row == fin->row && c->kind == fin->check_idx);
        } else {
            is_goal = (c->kind == 2) || (!has_goal_chk && c->kind == 1 && all_coins_got);
        }

        float cx = ox + c->x * scale, cy = oy + c->y * scale;
        float cw = c->w * scale, ch = c->h * scale;
        if (is_goal) {
            u32 gcol = all_coins_got ? C2D_Color32(0xFF, 0xE0, 0x38, 0xE8)
                                     : C2D_Color32(0x48, 0xD8, 0x90, 0xD8);
            u32 gbrd = all_coins_got ? C2D_Color32(0xFF, 0x90, 0x00, 0xFF)
                                     : C2D_Color32(0xFF, 0xA8, 0x20, 0xFF);
            C2D_DrawRectSolid(cx, cy, 0.1f, cw, ch, gbrd);
            if (cw > 4.0f && ch > 4.0f) {
                C2D_DrawRectSolid(cx + 2.0f, cy + 2.0f, 0.11f, cw - 4.0f, ch - 4.0f, gcol);
            }
        } else {
            C2D_DrawRectSolid(cx, cy, 0.1f, cw, ch, clrChk);
        }
    }

    float wall_ox = ox + depth_3d * 1.5f;
    for (uint16_t i = 0; i < lvl->num_walls; i++) {
        const Rect16* r = &lvl->walls[i];
        float rx = wall_ox + r->x * scale, ry = oy + r->y * scale;
        float rw = r->w * scale, rh = r->h * scale;
        if (rx + rw < -4.0f || rx > TOP_W + 4.0f || ry + rh < -4.0f || ry > TOP_H + 4.0f) continue;
        C2D_DrawRectSolid(rx - 1.5f, ry - 1.5f, 0.18f, rw + 3.0f, rh + 3.0f, clrWallOut);
    }
    for (uint16_t i = 0; i < lvl->num_walls; i++) {
        const Rect16* r = &lvl->walls[i];
        float rx = wall_ox + r->x * scale, ry = oy + r->y * scale;
        float rw = r->w * scale, rh = r->h * scale;
        if (rx + rw < -4.0f || rx > TOP_W + 4.0f || ry + rh < -4.0f || ry > TOP_H + 4.0f) continue;
        C2D_DrawRectSolid(rx, ry, 0.2f, rw + 0.4f, rh + 0.4f, clrWall);
    }

    for (uint16_t i = 0; i < lvl->num_doors; i++) {
        const DoorRect* d = &lvl->doors[i];
        if (!preview_only && gs) {
            if ((gs->is_whg3 || gs->is_whg4) && d->door_id == 255 && gs->gold_door_open) continue;
            if (d->door_id < MAX_DOORS && gs->door_open[d->door_id]) continue;
        }
        u32 dcol = (d->door_id == 255) ? C2D_Color32(0xFF, 0xD8, 0x20, 0xFF) : clrDoor;
        C2D_DrawRectSolid(wall_ox + d->x * scale - 1.0f, oy + d->y * scale - 1.0f, 0.23f,
                          d->w * scale + 2.0f, d->h * scale + 2.0f, clrWallOut);
        C2D_DrawRectSolid(wall_ox + d->x * scale, oy + d->y * scale, 0.25f,
                          d->w * scale, d->h * scale, dcol);
    }

    float ent_ox = ox + depth_3d * 3.0f;

    if (!preview_only && gs) {
        u8 alpha = (u8)((gs->player_alpha * 255) / 100);
        u32 fill = (render_skin_color(player_skin) & 0x00FFFFFFu) | ((u32)alpha << 24);
        u32 outline = C2D_Color32(0x00, 0x00, 0x00, alpha);
        float px = ent_ox + gs->player_x * scale;
        float py = oy + gs->player_y * scale;
        float hs = gs->player_draw_half_size * scale;
        C2D_DrawRectSolid(px - hs, py - hs, 0.6f, hs * 2.0f, hs * 2.0f, outline);
        C2D_DrawRectSolid(px - hs + 1.5f, py - hs + 1.5f, 0.62f,
                          hs * 2.0f - 3.0f, hs * 2.0f - 3.0f, fill);
        if (gs->is_whg4 && gs->in_water) {
            float ratio = (float)gs->water_ticks / (float)WATER_MAX_TICKS;
            float bw = hs * 2.4f;
            float bx = px - bw * 0.5f;
            float by = py - hs - 6.0f;
            C2D_DrawRectSolid(bx, by, 0.65f, bw, 3.5f, C2D_Color32(0, 0, 0, 200));
            C2D_DrawRectSolid(bx + 0.5f, by + 0.5f, 0.66f, (bw - 1.0f) * ratio, 2.5f, C2D_Color32(60, 210, 255, 255));
        }
    }

    // Batch textured quads for keys, coins, and enemies (locked 60 FPS on O3DS & N3DS)
    for (uint16_t i = 0; i < lvl->num_keys; i++) {
        if (!preview_only && gs) {
            if (gs->is_whg3 || gs->is_whg4) {
                if (lvl->keys[i].door_id < MAX_DOORS && gs->whg3_key_collected[lvl->keys[i].door_id]) continue;
            } else if (gs->key_collected[i]) {
                continue;
            }
        }
        draw_sprite_centered(sprites_key_idx,
                             ent_ox + lvl->keys[i].x * scale,
                             oy + lvl->keys[i].y * scale,
                             scale, 0.45f, 255);
    }

    for (uint16_t i = 0; i < lvl->num_diamonds; i++) {
        if (!preview_only && gs && gs->diamond_collected && gs->diamond_collected[i]) continue;
        float dx = ent_ox + lvl->diamonds[i].x * scale;
        float dy = oy + lvl->diamonds[i].y * scale;
        float dh = DIAMOND_RADIUS * scale;
        if (dh < 2.5f) dh = 2.5f;
        C2D_DrawRectSolid(dx - dh, dy - dh, 0.42f, dh * 2.0f, dh * 2.0f, C2D_Color32(20, 70, 120, 255));
        C2D_DrawRectSolid(dx - dh * 0.7f, dy - dh * 0.7f, 0.43f, dh * 1.4f, dh * 1.4f, C2D_Color32(80, 235, 255, 255));
    }

    for (uint16_t i = 0; i < lvl->num_coins; i++) {
        if (!preview_only && gs && gs->coin_collected[i]) continue;
        float cx = ent_ox + lvl->coins[i].x * scale;
        float cy = oy + lvl->coins[i].y * scale;
        if (cx < -12.0f || cx > TOP_W + 12.0f || cy < -12.0f || cy > TOP_H + 12.0f) continue;
        draw_sprite_centered(sprites_coin_idx, cx, cy, scale, 0.4f, 255);
    }

    float anim_f = (gs && !preview_only) ? gs->enemy_frame : 0.0f;
    for (uint16_t i = 0; i < lvl->num_enemies; i++) {
        float ex = 0, ey = 0;
        level_get_enemy_pos(&lvl->enemies[i], anim_f, &ex, &ey);
        float sxp = ent_ox + ex * scale;
        float syp = oy + ey * scale;
        if (sxp < -16.0f || sxp > TOP_W + 16.0f || syp < -16.0f || syp > TOP_H + 16.0f) continue;
        draw_sprite_centered(sprites_enemy_blue_idx, sxp, syp, scale, 0.5f, 255);
    }

    if (!preview_only && gs && s_fx_on) draw_particles(ent_ox, oy, scale);
}

void render_minimap(const LevelData* lvl, const GameState* gs,
                    float mx, float my, float mw, float mh) {
    if (!lvl || lvl->width == 0 || lvl->height == 0) return;

    /* meme echelle sur X et Y (avant : carte etiree), carte centree dans le cadre */
    float s = mw / (float)lvl->width;
    float sy = mh / (float)lvl->height;
    if (sy < s) s = sy;
    float ow = lvl->width * s, oh = lvl->height * s;
    float ox = mx + (mw - ow) * 0.5f;
    float oy = my + (mh - oh) * 0.5f;

    render_draw_card(mx - 2.0f, my - 2.0f, mw + 4.0f, mh + 4.0f,
                     C2D_Color32(0x12, 0x14, 0x24, 0xFF),
                     C2D_Color32(0x55, 0x60, 0x98, 0xFF));
    C2D_DrawRectSolid(ox, oy, 0.12f, ow, oh, C2D_Color32(0x1C, 0x20, 0x38, 0xFF));

    for (uint16_t i = 0; i < lvl->num_terrains; i++) {
        const TerrainRect* t = &lvl->terrains[i];
        u32 col;
        if (t->type == TERR_WATER)    col = C2D_Color32(0x2D, 0x82, 0xEB, 0xFF);
        else if (t->type == TERR_ICE) col = C2D_Color32(0xB9, 0xF0, 0xFF, 0xFF);
        else if (t->type == TERR_PIT) col = C2D_Color32(0x05, 0x05, 0x08, 0xFF);
        else                          col = C2D_Color32(0x6E, 0x72, 0x86, 0xFF);
        C2D_DrawRectSolid(ox + t->x * s, oy + t->y * s, 0.13f, t->w * s, t->h * s, col);
    }
    for (uint16_t i = 0; i < lvl->num_checks; i++) {
        const CheckRect* c = &lvl->checks[i];
        C2D_DrawRectSolid(ox + c->x * s, oy + c->y * s, 0.15f,
                          c->w * s, c->h * s, C2D_Color32(0x45, 0xC8, 0x55, 0xFF));
    }
    for (uint16_t i = 0; i < lvl->num_walls; i++) {
        const Rect16* r = &lvl->walls[i];
        C2D_DrawRectSolid(ox + r->x * s, oy + r->y * s, 0.2f,
                          r->w * s, r->h * s, C2D_Color32(0x78, 0x80, 0xC0, 0xFF));
    }
    for (uint16_t i = 0; i < lvl->num_doors; i++) {
        const DoorRect* d = &lvl->doors[i];
        if (gs) {
            if ((gs->is_whg3 || gs->is_whg4) && d->door_id == 255 && gs->gold_door_open) continue;
            if (d->door_id < MAX_DOORS && gs->door_open[d->door_id]) continue;
        }
        u32 dc = (d->door_id == 255) ? C2D_Color32(0xFF, 0xD8, 0x20, 0xFF) : C2D_Color32(0xFF, 0xA8, 0x20, 0xFF);
        C2D_DrawRectSolid(ox + d->x * s, oy + d->y * s, 0.22f, d->w * s, d->h * s, dc);
    }
    if (gs) {
        for (uint16_t i = 0; i < lvl->num_coins; i++) {
            if (gs->coin_collected && gs->coin_collected[i]) continue;
            C2D_DrawRectSolid(ox + lvl->coins[i].x * s - 1.0f, oy + lvl->coins[i].y * s - 1.0f,
                              0.25f, 2.0f, 2.0f, C2D_Color32(0xFF, 0xE0, 0x20, 0xFF));
        }
        for (uint16_t i = 0; i < lvl->num_diamonds; i++) {
            if (gs->diamond_collected && gs->diamond_collected[i]) continue;
            C2D_DrawRectSolid(ox + lvl->diamonds[i].x * s - 1.5f, oy + lvl->diamonds[i].y * s - 1.5f,
                              0.26f, 3.0f, 3.0f, C2D_Color32(0x50, 0xEB, 0xFF, 0xFF));
        }
        float px = ox + gs->player_x * s, py = oy + gs->player_y * s;
        C2D_DrawRectSolid(px - 3.0f, py - 3.0f, 0.3f, 6.0f, 6.0f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));
        C2D_DrawRectSolid(px - 2.0f, py - 2.0f, 0.31f, 4.0f, 4.0f, C2D_Color32(0xFF, 0x35, 0x35, 0xFF));
    }
}
