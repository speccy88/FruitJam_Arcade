#ifndef WIRE3D_H
#define WIRE3D_H
#include <stdbool.h>
typedef struct { float x,y,z; } Vec3; bool wire3d_project(Vec3 v, float *sx, float *sy); void wire3d_line(Vec3 a, Vec3 b, int c); void wire3d_draw_mesh(int mesh_id, float x,float y,float z,float s,int c); void wire3d_draw_corridor(float scroll);
#endif
