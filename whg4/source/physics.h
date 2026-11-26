#ifndef PHYSICS_H
#define PHYSICS_H

#include <stdbool.h>

bool phys_rect_overlap(float ax, float ay, float aw, float ah,
                       float bx, float by, float bw, float bh);

bool phys_square_circle_overlap(float sx, float sy, float half_size,
                                float cx, float cy, float radius);

float phys_resolve_axis(float pos, float other_pos, float half_size,
                        float rx, float ry, float rw, float rh,
                        bool is_x, float delta);

#endif // PHYSICS_H
