#ifndef WIRE3D_H
#define WIRE3D_H

#include <stdbool.h>

typedef struct {
    float x;
    float y;
    float z;
} Vec3;

void wire3d_set_view(float x, float y, float roll);
bool wire3d_project(Vec3 v, float *sx, float *sy);
void wire3d_line(Vec3 a, Vec3 b, int color);
void wire3d_draw_mesh(int mesh_id, float x, float y, float z, float scale, int color);
void wire3d_draw_mesh_rot(int mesh_id, float x, float y, float z, float scale,
                          float rotation, int color);
void wire3d_draw_corridor(float scroll);
void wire3d_draw_corridor_ex(float scroll, int floor_color, int ceiling_color,
                             int side_color);

#endif
