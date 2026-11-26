#include "physics.h"

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

bool phys_rect_overlap(float ax, float ay, float aw, float ah,
                       float bx, float by, float bw, float bh) {
    return (ax < bx + bw) && (ax + aw > bx) &&
           (ay < by + bh) && (ay + ah > by);
}

bool phys_square_circle_overlap(float sx, float sy, float half_size,
                                float cx, float cy, float radius) {
    float closest_x = clampf(cx, sx - half_size, sx + half_size);
    float closest_y = clampf(cy, sy - half_size, sy + half_size);
    float dx = cx - closest_x;
    float dy = cy - closest_y;
    return (dx * dx + dy * dy) < (radius * radius);
}

float phys_resolve_axis(float pos, float other_pos, float half_size,
                        float rx, float ry, float rw, float rh,
                        bool is_x, float delta) {
    float px = is_x ? pos : other_pos;
    float py = is_x ? other_pos : pos;
    if (!phys_rect_overlap(px - half_size, py - half_size, half_size * 2.0f, half_size * 2.0f,
                           rx, ry, rw, rh)) {
        return pos;
    }
    if (is_x) {
        if (delta > 0.0f) return rx - half_size;
        if (delta < 0.0f) return rx + rw + half_size;
    } else {
        if (delta > 0.0f) return ry - half_size;
        if (delta < 0.0f) return ry + rh + half_size;
    }
    return pos;
}
