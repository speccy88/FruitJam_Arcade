#include "wire3d.h"

#include "gfx.h"

#include <math.h>

#define NEAR_Z 0.8f

typedef struct {
    const Vec3 *vertices;
    const unsigned char (*edges)[2];
    int edge_count;
} Mesh;

static const Vec3 cube_v[] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1},
};
static const unsigned char cube_e[][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
    {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7},
};

static const Vec3 pyr_v[] = {
    {-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}, {0, 0, 1.5f},
};
static const unsigned char pyr_e[][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, {0, 4}, {1, 4}, {2, 4}, {3, 4},
};

static const Vec3 bug_v[] = {
    {-1, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0, 0, -1},
    {-0.7f, 0.7f, 0}, {0.7f, 0.7f, 0}, {-0.7f, -0.7f, 0}, {0.7f, -0.7f, 0},
};
static const unsigned char bug_e[][2] = {
    {0, 1}, {2, 3}, {0, 4}, {4, 2}, {1, 5}, {5, 2},
    {0, 6}, {6, 3}, {1, 7}, {7, 3},
};

static const Vec3 diamond_v[] = {
    {-1.2f, 0, 0}, {1.2f, 0, 0}, {0, -1.2f, 0}, {0, 1.2f, 0},
    {0, 0, -1.3f}, {0, 0, 1.3f},
};
static const unsigned char diamond_e[][2] = {
    {0, 2}, {2, 1}, {1, 3}, {3, 0},
    {0, 4}, {1, 4}, {2, 4}, {3, 4},
    {0, 5}, {1, 5}, {2, 5}, {3, 5},
};

static const Vec3 wing_v[] = {
    {-1.5f, 0, 0}, {-0.35f, -0.35f, -0.7f}, {0, 0, 1.2f},
    {0.35f, -0.35f, -0.7f}, {1.5f, 0, 0}, {0, 0.65f, -0.25f},
    {0, -0.7f, -0.4f},
};
static const unsigned char wing_e[][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 0},
    {0, 6}, {6, 4}, {1, 6}, {3, 6}, {2, 5},
};

static float view_x;
static float view_y;
static float view_roll;
static float view_cos = 1.0f;
static float view_sin;

static Mesh mesh_for_id(int id) {
    switch (id) {
        case 1: return (Mesh){pyr_v, pyr_e, (int)(sizeof(pyr_e) / sizeof(pyr_e[0]))};
        case 2: return (Mesh){bug_v, bug_e, (int)(sizeof(bug_e) / sizeof(bug_e[0]))};
        case 4: return (Mesh){diamond_v, diamond_e, (int)(sizeof(diamond_e) / sizeof(diamond_e[0]))};
        case 5: return (Mesh){wing_v, wing_e, (int)(sizeof(wing_e) / sizeof(wing_e[0]))};
        default: return (Mesh){cube_v, cube_e, (int)(sizeof(cube_e) / sizeof(cube_e[0]))};
    }
}

void wire3d_set_view(float x, float y, float roll) {
    view_x = x;
    view_y = y;
    view_roll = roll;
    if (roll != 0.0f) {
        view_cos = cosf(roll);
        view_sin = sinf(roll);
    } else {
        view_cos = 1.0f;
        view_sin = 0.0f;
    }
}

bool wire3d_project(Vec3 v, float *sx, float *sy) {
    if (v.z <= NEAR_Z) return false;
    const float f = 58.0f / v.z;
    float px = v.x * f;
    float py = -v.y * f;
    if (view_roll != 0.0f) {
        const float rx = px * view_cos - py * view_sin;
        py = px * view_sin + py * view_cos;
        px = rx;
    }
    *sx = 64.0f + px + view_x;
    *sy = 64.0f + py + view_y;
    return true;
}

static Vec3 clipped_point(Vec3 a, Vec3 b) {
    const float target_z = NEAR_Z + 0.001f;
    const float dz = b.z - a.z;
    const float t = dz == 0.0f ? 0.0f : (target_z - a.z) / dz;
    return (Vec3){
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        target_z,
    };
}

void wire3d_line(Vec3 a, Vec3 b, int color) {
    if (a.z <= NEAR_Z && b.z <= NEAR_Z) return;
    if (a.z <= NEAR_Z) a = clipped_point(a, b);
    if (b.z <= NEAR_Z) b = clipped_point(b, a);
    float x0, y0, x1, y1;
    if (!wire3d_project(a, &x0, &y0) || !wire3d_project(b, &x1, &y1)) return;
    gfx_line((int)x0, (int)y0, (int)x1, (int)y1, color);
}

static Vec3 transform_vertex(Vec3 v, float x, float y, float z, float scale,
                             float cs, float sn) {
    const float rx = v.x * cs - v.y * sn;
    const float ry = v.x * sn + v.y * cs;
    return (Vec3){x + rx * scale, y + ry * scale, z + v.z * scale};
}

static void draw_simple_mesh(int id, float x, float y, float z, float scale,
                             float rotation, int color) {
    const Mesh mesh = mesh_for_id(id);
    const float cs = cosf(rotation);
    const float sn = sinf(rotation);
    for (int i = 0; i < mesh.edge_count; ++i) {
        const Vec3 a = transform_vertex(mesh.vertices[mesh.edges[i][0]], x, y, z, scale, cs, sn);
        const Vec3 b = transform_vertex(mesh.vertices[mesh.edges[i][1]], x, y, z, scale, cs, sn);
        wire3d_line(a, b, color);
    }
}

void wire3d_draw_mesh_rot(int id, float x, float y, float z, float scale,
                          float rotation, int color) {
    if (id == 3) {
        draw_simple_mesh(4, x, y, z, scale * 1.55f, rotation, color);
        draw_simple_mesh(0, x, y, z + scale * 0.45f, scale * 0.9f, -rotation * 1.4f, color);
        draw_simple_mesh(1, x, y, z + scale * 1.65f, scale * 0.75f, rotation * 1.8f, color);
        return;
    }
    draw_simple_mesh(id, x, y, z, scale, rotation, color);
}

void wire3d_draw_mesh(int id, float x, float y, float z, float scale, int color) {
    wire3d_draw_mesh_rot(id, x, y, z, scale, 0.0f, color);
}

void wire3d_draw_corridor_ex(float scroll, int floor_color, int ceiling_color,
                             int side_color) {
    float wrapped = fmodf(scroll, 2.0f);
    if (wrapped < 0.0f) wrapped += 2.0f;
    for (int i = 0; i < 15; ++i) {
        const float z = 1.1f + (float)i * 2.0f - wrapped;
        wire3d_line((Vec3){-4, -2, z}, (Vec3){4, -2, z}, floor_color);
        wire3d_line((Vec3){-4, 2, z}, (Vec3){4, 2, z}, ceiling_color);
        wire3d_line((Vec3){-4, -2, z}, (Vec3){-4, 2, z}, side_color);
        wire3d_line((Vec3){4, -2, z}, (Vec3){4, 2, z}, side_color);
    }
    for (int x = -4; x <= 4; x += 2) {
        wire3d_line((Vec3){(float)x, -2, 0.82f}, (Vec3){(float)x, -2, 31}, floor_color);
        wire3d_line((Vec3){(float)x, 2, 0.82f}, (Vec3){(float)x, 2, 31}, ceiling_color);
    }
    for (int y = -2; y <= 2; y += 2) {
        wire3d_line((Vec3){-4, (float)y, 0.82f}, (Vec3){-4, (float)y, 31}, side_color);
        wire3d_line((Vec3){4, (float)y, 0.82f}, (Vec3){4, (float)y, 31}, side_color);
    }
}

void wire3d_draw_corridor(float scroll) {
    wire3d_draw_corridor_ex(scroll, 5, 1, 13);
}
