#include "native_game.h"

#include "audio.h"
#include "gfx.h"
#include "input.h"
#include "native_io.h"
#include "native_pad.h"
#include "wire3d.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_ENEMIES 24
#define MAX_THREATS 18
#define MAX_PICKUPS 6
#define MAX_FX 72
#define MAX_POPUPS 8
#define MAX_SETPIECES 12
#define MAG_SIZE 8
#define MAX_SHIELD 100
#define TAU 6.28318530718f

#define XINPUT_GAMEPAD_DPAD_UP 0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN 0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT 0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT 0x0008
#define XINPUT_GAMEPAD_START 0x0010
#define XINPUT_GAMEPAD_BACK 0x0020
#define XINPUT_GAMEPAD_A 0x1000
#define XINPUT_GAMEPAD_B 0x2000
#define XINPUT_GAMEPAD_Y 0x8000

#define HID_KEY_Q 20

typedef enum {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_WAVE_CLEAR,
    STATE_GAMEOVER,
} GameState;

typedef enum {
    RUN_MODE_ARCADE,
    RUN_MODE_GOD_TEST,
} RunMode;

typedef enum {
    ENEMY_SCOUT,
    ENEMY_WEAVER,
    ENEMY_TANK,
    ENEMY_SNIPER,
    ENEMY_INTERCEPTOR,
    ENEMY_STALKER,
    ENEMY_GUNSHIP,
    ENEMY_BOSS,
} EnemyType;

typedef enum {
    THREAT_BOLT,
    THREAT_PLASMA,
    THREAT_MISSILE,
    THREAT_LANCE,
    THREAT_SHARD,
} ThreatType;

typedef enum {
    PICKUP_SHIELD,
    PICKUP_AMMO,
    PICKUP_PULSE,
} PickupType;

typedef enum {
    FX_SPARK,
    FX_RING,
    FX_DEBRIS,
} FxType;

typedef enum {
    RAIL_PURSUIT,
    RAIL_BANK_LEFT,
    RAIL_BANK_RIGHT,
    RAIL_CLIMB,
    RAIL_DIVE,
    RAIL_EVADE_LEFT,
    RAIL_EVADE_RIGHT,
    RAIL_COVER,
    RAIL_DOGFIGHT,
    RAIL_FLYBY,
} RailCue;

typedef enum {
    SETPIECE_WINGMAN,
    SETPIECE_HAZARD,
    SETPIECE_DEBRIS,
    SETPIECE_CAPITAL,
    SETPIECE_CREATURE,
    SETPIECE_STRUCTURE,
} SetPieceType;

typedef enum {
    ENV_SPACE_CHASE,
    ENV_MOUNTAIN_RUN,
    ENV_ALIEN_FOREST,
    ENV_UNDERWATER,
    ENV_CRYSTAL_CAVERN,
    ENV_ORBITAL_RUINS,
} EnvironmentType;

typedef struct {
    bool alive;
    EnemyType type;
    uint8_t mesh_id;
    float x;
    float y;
    float z;
    float base_x;
    float base_y;
    float vz;
    float scale;
    float phase;
    float attack_timer;
    float hit_flash;
    int hp;
    int max_hp;
    int score_value;
    int color;
    bool elite;
    float age;
    float entry_x;
    float entry_y;
} Enemy;

typedef struct {
    bool active;
    ThreatType type;
    float x;
    float y;
    float z;
    float vz;
    float phase;
    float drift_x;
    float drift_y;
    int color;
    int damage;
} Threat;

typedef struct {
    bool active;
    PickupType type;
    float x;
    float y;
    float z;
    float phase;
} Pickup;

typedef struct {
    bool active;
    FxType type;
    float x;
    float y;
    float vx;
    float vy;
    float life;
    float max_life;
    int color;
    int size;
} Fx;

typedef struct {
    bool active;
    float x;
    float y;
    float life;
    int color;
    char text[16];
} Popup;

typedef struct {
    bool active;
    SetPieceType type;
    uint8_t mesh_id;
    float x;
    float y;
    float z;
    float vx;
    float vy;
    float vz;
    float scale;
    float rotation;
    float spin;
    int color;
} SetPiece;

typedef struct {
    RailCue cue;
    int route_step;
    float cue_time;
    float cue_duration;
    float total_time;
    float view_x;
    float view_y;
    float roll;
    float bend_x;
    float bend_y;
    float speed;
    float cover;
    float target_view_x;
    float target_view_y;
    float target_roll;
    float target_bend_x;
    float target_bend_y;
    float target_speed;
    float target_cover;
    float project_x;
    float project_y;
    float project_roll;
    float project_cos;
    float project_sin;
    float callout_timer;
    bool protected;
    char callout[24];
} RailMotion;

static Enemy enemies[MAX_ENEMIES];
static Threat threats[MAX_THREATS];
static Pickup pickups[MAX_PICKUPS];
static Fx effects[MAX_FX];
static Popup popups[MAX_POPUPS];
static SetPiece setpieces[MAX_SETPIECES];
static int draw_order[MAX_ENEMIES];
static RailMotion rail;

static GameState state;
static RunMode selected_mode;
static RunMode active_mode;
static int score;
static int high_score;
static int ammo;
static int lives;
static int shield;
static int wave;
static int credits;
static int wave_spawned;
static int wave_resolved;
static int wave_quota;
static int wave_bonus;
static int shots_fired;
static int shots_hit;
static int total_kills;
static int combo_hits;
static int best_combo;
static int multiplier;
static float aim_x;
static float aim_y;
static float spawn_timer;
static float scroll;
static float wave_timer;
static float wave_banner;
static float reload_timer;
static int reload_stage;
static float combo_timer;
static float pulse_meter;
static float music_timer;
static int music_step;
static float invulnerable_timer;
static float muzzle_flash;
static float hit_marker;
static float damage_flash;
static float pulse_flash;
static float screen_shake;
static float camera_shake_x;
static float camera_shake_y;
static float title_time;
static bool prev_fire;
static bool prev_reload;
static bool prev_start;
static bool prev_back;
static bool prev_pulse;
static bool prev_coin;
static bool prev_mode_left;
static bool prev_mode_right;
static bool debug_overlay;
static uint32_t rng_state;
static uint32_t fps_frames;
static uint32_t last_fps_ms;
static float fps;

static const int floor_colors[] = {5, 3, 4, 2, 5, 1};
static const int ceiling_colors[] = {1, 13, 2, 5, 1, 13};
static const int side_colors[] = {13, 12, 9, 14, 11, 8};

#define RAIL_ROUTE_LENGTH 9
static const RailCue rail_routes[4][RAIL_ROUTE_LENGTH] = {
    {RAIL_PURSUIT, RAIL_BANK_LEFT, RAIL_DOGFIGHT, RAIL_CLIMB,
     RAIL_FLYBY, RAIL_DIVE, RAIL_PURSUIT, RAIL_EVADE_RIGHT, RAIL_BANK_RIGHT},
    {RAIL_PURSUIT, RAIL_DIVE, RAIL_FLYBY, RAIL_BANK_RIGHT,
     RAIL_DOGFIGHT, RAIL_CLIMB, RAIL_PURSUIT, RAIL_EVADE_LEFT, RAIL_BANK_LEFT},
    {RAIL_PURSUIT, RAIL_BANK_RIGHT, RAIL_DOGFIGHT, RAIL_CLIMB,
     RAIL_FLYBY, RAIL_DIVE, RAIL_PURSUIT, RAIL_EVADE_LEFT, RAIL_BANK_LEFT},
    {RAIL_PURSUIT, RAIL_BANK_LEFT, RAIL_DOGFIGHT, RAIL_COVER,
     RAIL_FLYBY, RAIL_CLIMB, RAIL_BANK_RIGHT, RAIL_EVADE_LEFT, RAIL_DIVE},
};

static bool project_world(float x, float y, float z, float *sx, float *sy);
static void stop_music(void);
static int part_size(int radius, int numerator, int denominator);
static void draw_engine(int x, int y, int radius, int color);
static void draw_interceptor_ship(int x, int y, int r, int accent, float phase);

static float clampf(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int clampi(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static uint32_t random_u32(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

static float random_float(float low, float high) {
    const float unit = (float)(random_u32() & 0xffffu) / 65535.0f;
    return low + (high - low) * unit;
}

static void tone(int channel, float frequency, int duration_ms, int waveform) {
    audio_tone(channel, frequency, duration_ms, waveform);
}

static void voice(int channel, float frequency, float end_frequency,
                  int duration_ms, int waveform, int volume, int pan,
                  int attack_ms, int release_ms) {
    audio_tone_ex(channel, frequency, end_frequency, duration_ms, waveform,
                  volume, pan, attack_ms, release_ms);
}

static bool god_mode(void) {
    return active_mode == RUN_MODE_GOD_TEST;
}

static float normalized_axis(int16_t value) {
    float result = value < 0 ? (float)value / 32768.0f : (float)value / 32767.0f;
    const float sign = result < 0.0f ? -1.0f : 1.0f;
    result = fabsf(result);
    if (result < 0.18f) return 0.0f;
    result = (result - 0.18f) / 0.82f;
    result = result * 0.3f + result * result * 0.7f;
    return result * sign;
}

static int live_enemy_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].alive) ++count;
    }
    return count;
}

int native_game_live_enemies(void) {
    return live_enemy_count();
}

int native_game_score(void) {
    return score;
}

int native_game_wave(void) {
    return wave;
}

int native_game_state(void) {
    return (int)state;
}

bool native_game_god_mode(void) {
    return god_mode();
}

int native_game_ammo(void) {
    return ammo;
}

int native_game_lives(void) {
    return lives;
}

int native_game_shield(void) {
    return shield;
}

bool native_game_demo_target(int *x, int *y) {
    int best = -1;
    float best_z = 1000.0f;
    for (int i = 0; i < MAX_THREATS; ++i) {
        if (threats[i].active && threats[i].z < best_z) {
            best = i;
            best_z = threats[i].z;
        }
    }
    if (best >= 0) {
        float sx, sy;
        if (project_world(threats[best].x, threats[best].y, threats[best].z, &sx, &sy)) {
            *x = (int)sx;
            *y = (int)sy;
            return true;
        }
    }
    best = -1;
    best_z = 1000.0f;
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].alive && enemies[i].z < best_z) {
            best = i;
            best_z = enemies[i].z;
        }
    }
    if (best >= 0) {
        float sx, sy;
        if (project_world(enemies[best].x, enemies[best].y, enemies[best].z, &sx, &sy)) {
            *x = (int)sx;
            *y = (int)sy;
            return true;
        }
    }
    return false;
}

static void clear_entities(void) {
    memset(enemies, 0, sizeof(enemies));
    memset(threats, 0, sizeof(threats));
    memset(pickups, 0, sizeof(pickups));
    memset(effects, 0, sizeof(effects));
    memset(popups, 0, sizeof(popups));
    memset(setpieces, 0, sizeof(setpieces));
}

static void add_score(int points) {
    score += points;
    if (!god_mode() && score > high_score) high_score = score;
}

static void add_popup(float x, float y, int color, const char *text) {
    for (int i = 0; i < MAX_POPUPS; ++i) {
        if (!popups[i].active) {
            popups[i].active = true;
            popups[i].x = x;
            popups[i].y = y;
            popups[i].life = 0.8f;
            popups[i].color = color;
            snprintf(popups[i].text, sizeof(popups[i].text), "%s", text);
            return;
        }
    }
}

static void add_score_popup(float x, float y, int points, int color) {
    char text[16];
    snprintf(text, sizeof(text), "+%d", points);
    add_popup(x, y, color, text);
}

static void add_fx(FxType type, float x, float y, float vx, float vy,
                   float life, int color, int size) {
    for (int i = 0; i < MAX_FX; ++i) {
        if (!effects[i].active) {
            effects[i] = (Fx){true, type, x, y, vx, vy, life, life, color, size};
            return;
        }
    }
}

static void add_ring(float x, float y, int color, int size) {
    add_fx(FX_RING, x, y, 0.0f, 0.0f, 0.34f, color, size);
}

static void add_burst(float x, float y, int color, int count, float speed) {
    for (int i = 0; i < count; ++i) {
        const float angle = random_float(0.0f, TAU);
        const float velocity = random_float(speed * 0.45f, speed);
        add_fx(i % 3 == 0 ? FX_DEBRIS : FX_SPARK, x, y,
               cosf(angle) * velocity, sinf(angle) * velocity,
               random_float(0.28f, 0.65f), i % 4 == 0 ? 7 : color,
               i % 3 == 0 ? 2 : 1);
    }
}

static EnvironmentType environment_for_wave(int number) {
    const int index = number > 0 ? ((number - 1) / 2) % 6 : 0;
    return (EnvironmentType)index;
}

static EnvironmentType current_environment(void) {
    return environment_for_wave(wave);
}

static const char *environment_label(EnvironmentType environment) {
    switch (environment) {
        case ENV_SPACE_CHASE: return "ASTEROID CHASE";
        case ENV_MOUNTAIN_RUN: return "MOUNTAIN RUN";
        case ENV_ALIEN_FOREST: return "ALIEN FOREST";
        case ENV_UNDERWATER: return "ABYSSAL SEA";
        case ENV_CRYSTAL_CAVERN: return "CRYSTAL CAVERN";
        case ENV_ORBITAL_RUINS: return "ORBITAL RUINS";
        default: return "UNKNOWN SECTOR";
    }
}

static const char *environment_name(void) {
    return environment_label(current_environment());
}

static float environment_speed_scale(void) {
    switch (current_environment()) {
        case ENV_SPACE_CHASE: return 1.12f;
        case ENV_MOUNTAIN_RUN: return 1.0f;
        case ENV_ALIEN_FOREST: return 0.9f;
        case ENV_UNDERWATER: return 0.76f;
        case ENV_CRYSTAL_CAVERN: return 0.86f;
        case ENV_ORBITAL_RUINS: return 1.04f;
        default: return 1.0f;
    }
}

static Vec3 curved_world_point(float x, float y, float z) {
    return wire3d_curve_point((Vec3){x, y, z}, rail.bend_x, rail.bend_y);
}

static bool project_world(float x, float y, float z, float *sx, float *sy) {
    const Vec3 point = curved_world_point(x, y, z);
    return wire3d_project_view_rot(point,
                                   rail.project_x + camera_shake_x,
                                   rail.project_y + camera_shake_y,
                                   rail.project_cos, rail.project_sin, sx, sy);
}

static bool project_draw_world(float x, float y, float z, float *sx, float *sy) {
    return wire3d_project(curved_world_point(x, y, z), sx, sy);
}

static void reset_rail_motion(void) {
    memset(&rail, 0, sizeof(rail));
    rail.cue = RAIL_PURSUIT;
    rail.speed = 1.0f;
    rail.target_speed = 1.0f;
    rail.project_cos = 1.0f;
}

static bool spawn_setpiece(SetPieceType type, int mesh_id,
                           float x, float y, float z,
                           float vx, float vy, float vz,
                           float scale, float spin, int color) {
    for (int i = 0; i < MAX_SETPIECES; ++i) {
        if (setpieces[i].active) continue;
        setpieces[i] = (SetPiece){
            .active = true,
            .type = type,
            .mesh_id = (uint8_t)mesh_id,
            .x = x,
            .y = y,
            .z = z,
            .vx = vx,
            .vy = vy,
            .vz = vz,
            .scale = scale,
            .rotation = (float)(i + 1) * 0.61f,
            .spin = spin,
            .color = color,
        };
        return true;
    }
    return false;
}

static void spawn_cue_setpieces(RailCue cue) {
    if (cue == RAIL_PURSUIT && (rail.route_step <= 1 || live_enemy_count() == 0)) {
        (void)spawn_setpiece(SETPIECE_WINGMAN, 5, -2.6f, -1.0f, 2.5f,
                             0.38f, 0.08f, -5.4f, 0.42f, -1.3f, 11);
        (void)spawn_setpiece(SETPIECE_WINGMAN, 5, 2.6f, -0.8f, 2.8f,
                             -0.38f, 0.05f, -5.1f, 0.42f, 1.15f, 12);
        return;
    }
    if (cue == RAIL_DOGFIGHT) {
        const float direction = rail.route_step & 1 ? 1.0f : -1.0f;
        (void)spawn_setpiece(SETPIECE_WINGMAN, 5, direction * 5.8f, -0.4f, 11.0f,
                             -direction * 2.6f, 0.12f, 1.7f, 0.65f,
                             direction * 1.8f, 11);
        (void)spawn_setpiece(SETPIECE_WINGMAN, 5, -direction * 4.8f, 1.2f, 13.5f,
                             direction * 2.2f, -0.16f, 1.9f, 0.52f,
                             -direction * 1.5f, 12);
        return;
    }
    if (cue == RAIL_FLYBY) {
        const EnvironmentType environment = current_environment();
        const float direction = rail.route_step & 1 ? 1.0f : -1.0f;
        const SetPieceType type =
            environment == ENV_ALIEN_FOREST || environment == ENV_UNDERWATER
                ? SETPIECE_CREATURE
                : environment == ENV_MOUNTAIN_RUN || environment == ENV_CRYSTAL_CAVERN
                    ? SETPIECE_STRUCTURE
                    : SETPIECE_CAPITAL;
        const int color = environment == ENV_UNDERWATER ? 12 :
                          environment == ENV_ALIEN_FOREST ? 11 :
                          environment == ENV_CRYSTAL_CAVERN ? 14 : 6;
        (void)spawn_setpiece(type, (int)environment,
                             direction * 7.5f, environment == ENV_UNDERWATER ? 0.8f : -0.2f,
                             13.5f, -direction * 1.75f, 0.08f, 1.35f,
                             type == SETPIECE_CAPITAL ? 2.8f : 2.25f,
                             direction * 0.28f, color);
        return;
    }
    if (cue == RAIL_EVADE_LEFT || cue == RAIL_EVADE_RIGHT) {
        const float direction = cue == RAIL_EVADE_LEFT ? 1.0f : -1.0f;
        (void)spawn_setpiece(SETPIECE_HAZARD, (int)current_environment(),
                             direction * 0.65f, 0.1f, 14.5f,
                             direction * 0.22f, -0.05f, 7.8f, 2.35f,
                             direction * 1.45f, 5);
        for (int i = 0; i < 2; ++i) {
            const float side = i & 1 ? 1.0f : -1.0f;
            (void)spawn_setpiece(SETPIECE_DEBRIS, (int)current_environment(),
                                 side * (2.8f + (float)i * 0.55f),
                                 -1.4f + (float)i * 0.85f,
                                 11.0f + (float)i * 2.2f,
                                 -side * 0.3f, side * 0.1f, 6.8f,
                                 0.42f + (float)i * 0.08f,
                                 side * (1.8f + (float)i * 0.25f), 13);
        }
    }
}

static void set_rail_callout(const char *text) {
    snprintf(rail.callout, sizeof(rail.callout), "%s", text);
    rail.callout_timer = 0.9f;
}

static float rail_cue_duration(RailCue cue) {
    switch (cue) {
        case RAIL_PURSUIT: return 3.6f;
        case RAIL_BANK_LEFT:
        case RAIL_BANK_RIGHT: return 3.0f;
        case RAIL_CLIMB: return 2.9f;
        case RAIL_DIVE: return 2.7f;
        case RAIL_EVADE_LEFT:
        case RAIL_EVADE_RIGHT: return 2.35f;
        case RAIL_COVER: return 2.6f;
        case RAIL_DOGFIGHT: return 3.4f;
        case RAIL_FLYBY: return 3.15f;
        default: return 3.5f;
    }
}

static void start_rail_cue(RailCue cue) {
    rail.cue = cue;
    rail.cue_time = 0.0f;
    rail.cue_duration = rail_cue_duration(cue);
    rail.target_view_x = 0.0f;
    rail.target_view_y = 0.0f;
    rail.target_roll = 0.0f;
    rail.target_bend_x = 0.0f;
    rail.target_bend_y = 0.0f;
    rail.target_speed = 1.0f;
    rail.target_cover = 0.0f;
    rail.callout_timer = 0.0f;
    rail.protected = false;

    switch (cue) {
        case RAIL_PURSUIT:
            rail.target_speed = 1.55f;
            if (rail.route_step > 1) set_rail_callout("PILOT: IN PURSUIT");
            break;
        case RAIL_BANK_LEFT:
            rail.target_view_x = 6.0f;
            rail.target_roll = -0.14f;
            rail.target_bend_x = -7.5f;
            rail.target_speed = 1.16f;
            set_rail_callout("PILOT: BANK LEFT");
            break;
        case RAIL_BANK_RIGHT:
            rail.target_view_x = -6.0f;
            rail.target_roll = 0.14f;
            rail.target_bend_x = 7.5f;
            rail.target_speed = 1.16f;
            set_rail_callout("PILOT: BANK RIGHT");
            break;
        case RAIL_CLIMB:
            rail.target_view_y = 5.0f;
            rail.target_roll = -0.035f;
            rail.target_bend_y = 6.5f;
            rail.target_speed = 1.08f;
            set_rail_callout("PILOT: CLIMB!");
            break;
        case RAIL_DIVE:
            rail.target_view_y = -4.0f;
            rail.target_roll = 0.035f;
            rail.target_bend_y = -6.5f;
            rail.target_speed = 1.36f;
            set_rail_callout("PILOT: DIVE!");
            break;
        case RAIL_EVADE_LEFT:
            rail.target_view_x = 15.0f;
            rail.target_view_y = 2.0f;
            rail.target_roll = -0.22f;
            rail.target_bend_x = -9.0f;
            rail.target_speed = 1.75f;
            set_rail_callout("PILOT: EVADE LEFT!");
            break;
        case RAIL_EVADE_RIGHT:
            rail.target_view_x = -15.0f;
            rail.target_view_y = 2.0f;
            rail.target_roll = 0.22f;
            rail.target_bend_x = 9.0f;
            rail.target_speed = 1.75f;
            set_rail_callout("PILOT: EVADE RIGHT!");
            break;
        case RAIL_COVER:
            rail.target_view_y = 10.0f;
            rail.target_bend_y = 6.0f;
            rail.target_speed = 0.36f;
            rail.target_cover = 1.0f;
            set_rail_callout("PILOT: TAKE COVER");
            break;
        case RAIL_DOGFIGHT:
            rail.target_speed = 1.34f;
            rail.target_roll = rail.route_step & 1 ? 0.08f : -0.08f;
            rail.target_bend_x = rail.route_step & 1 ? 3.5f : -3.5f;
            set_rail_callout("WING: BANDITS CLOSE!");
            break;
        case RAIL_FLYBY:
            rail.target_view_x = rail.route_step & 1 ? -7.0f : 7.0f;
            rail.target_roll = rail.route_step & 1 ? 0.11f : -0.11f;
            rail.target_speed = 0.92f;
            set_rail_callout("PILOT: TRAFFIC CLOSE");
            break;
    }

    if (state == STATE_PLAYING) {
        spawn_cue_setpieces(cue);
        if (cue == RAIL_EVADE_LEFT || cue == RAIL_EVADE_RIGHT) {
            voice(2, 180.0f, 72.0f, 240, WAVE_SAW, 190, 0, 2, 120);
            screen_shake = fmaxf(screen_shake, 1.8f);
        } else if (cue == RAIL_CLIMB || cue == RAIL_DIVE) {
            voice(2, cue == RAIL_CLIMB ? 330.0f : 520.0f,
                  cue == RAIL_CLIMB ? 620.0f : 220.0f,
                  150, WAVE_TRIANGLE, 170, 0, 4, 70);
        } else if (cue == RAIL_DOGFIGHT) {
            voice(2, 640.0f, 420.0f, 120, WAVE_SAW, 145, -42, 2, 60);
        } else if (cue == RAIL_FLYBY) {
            voice(2, 94.0f, 62.0f, 520, WAVE_SINE, 135, 48, 35, 240);
        }
    }
}

static int rail_route_variant(void) {
    if (wave > 0 && wave % 5 == 0) return 3;
    return wave > 0 ? (wave - 1) % 3 : 0;
}

static void begin_wave_route(void) {
    rail.route_step = 1;
    start_rail_cue(rail_routes[rail_route_variant()][0]);
}

static void advance_rail_route(void) {
    const int variant = rail_route_variant();
    const int index = rail.route_step % RAIL_ROUTE_LENGTH;
    ++rail.route_step;
    start_rail_cue(rail_routes[variant][index]);
}

static void update_rail_motion(float dt) {
    if (state == STATE_PAUSED || state == STATE_GAMEOVER) {
        if (state == STATE_GAMEOVER) {
            rail.callout_timer = 0.0f;
            rail.protected = false;
        }
        return;
    }

    rail.total_time += dt;
    if (rail.callout_timer > 0.0f) rail.callout_timer -= dt;

    if (state == STATE_TITLE) {
        rail.project_x = sinf(rail.total_time * 0.42f) * 1.8f;
        rail.project_y = sinf(rail.total_time * 0.31f) * 0.8f;
        rail.project_roll = sinf(rail.total_time * 0.28f) * 0.018f;
        rail.project_cos = cosf(rail.project_roll);
        rail.project_sin = sinf(rail.project_roll);
        rail.bend_x = sinf(rail.total_time * 0.23f) * 1.2f;
        rail.bend_y = 0.0f;
        rail.speed = 1.0f;
        return;
    }

    if (state == STATE_PLAYING) {
        rail.cue_time += dt;
        if (rail.cue_time >= rail.cue_duration) advance_rail_route();
    } else if (state == STATE_WAVE_CLEAR) {
        rail.cue_time += dt;
    }

    float target_x = rail.target_view_x;
    float target_y = rail.target_view_y;
    float target_roll = rail.target_roll;
    float target_bend_x = rail.target_bend_x;
    float target_bend_y = rail.target_bend_y;
    if (rail.cue == RAIL_PURSUIT) {
        const float sway = sinf(rail.cue_time * 0.92f);
        target_x -= sway * 1.7f;
        target_roll += sway * 0.04f;
        target_bend_x += sway * 2.6f;
        target_y += sinf(rail.cue_time * 1.35f) * 0.8f;
    }

    const float response = clampf(dt *
        (rail.cue == RAIL_EVADE_LEFT || rail.cue == RAIL_EVADE_RIGHT ? 4.6f : 2.7f),
        0.0f, 1.0f);
    rail.view_x += (target_x - rail.view_x) * response;
    rail.view_y += (target_y - rail.view_y) * response;
    rail.roll += (target_roll - rail.roll) * response;
    rail.bend_x += (target_bend_x - rail.bend_x) * response;
    rail.bend_y += (target_bend_y - rail.bend_y) * response;
    rail.speed += (rail.target_speed - rail.speed) * response;
    rail.cover += (rail.target_cover - rail.cover) * clampf(dt * 4.2f, 0.0f, 1.0f);

    const float ride_bob = 1.0f - rail.cover * 0.8f;
    rail.project_x = rail.view_x + sinf(rail.total_time * 1.73f) * 0.65f * ride_bob;
    rail.project_y = rail.view_y + sinf(rail.total_time * 2.07f) * 0.45f * ride_bob;
    rail.project_roll = rail.roll + sinf(rail.total_time * 1.11f) * 0.008f * ride_bob;
    rail.project_cos = cosf(rail.project_roll);
    rail.project_sin = sinf(rail.project_roll);
    const bool evade_window =
        (rail.cue == RAIL_EVADE_LEFT || rail.cue == RAIL_EVADE_RIGHT) &&
        rail.cue_time > rail.cue_duration * 0.46f &&
        rail.cue_time < rail.cue_duration * 0.94f;
    rail.protected = evade_window || rail.cover > 0.55f;
}

static void update_setpieces(float dt) {
    for (int i = 0; i < MAX_SETPIECES; ++i) {
        SetPiece *piece = &setpieces[i];
        if (!piece->active) continue;
        piece->x += piece->vx * dt;
        piece->y += piece->vy * dt;
        piece->z -= piece->vz * dt;
        piece->rotation += piece->spin * dt;

        if (piece->type == SETPIECE_WINGMAN || piece->type == SETPIECE_CAPITAL ||
            piece->type == SETPIECE_CREATURE || piece->type == SETPIECE_STRUCTURE) {
            if (piece->z > 32.0f || piece->z < 0.82f || fabsf(piece->x) > 12.0f) {
                piece->active = false;
            }
            continue;
        }
        if (piece->z > 0.88f) continue;

        if (piece->type == SETPIECE_HAZARD) {
            float sx = 64.0f;
            float sy = 64.0f;
            piece->z = 0.92f;
            if (project_world(piece->x, piece->y, piece->z, &sx, &sy)) {
                add_burst(clampf(sx, 8.0f, 120.0f), clampf(sy, 12.0f, 112.0f),
                          13, 14, 58.0f);
            }
            add_popup(46.0f, 48.0f, 10, "NEAR MISS");
            screen_shake = fmaxf(screen_shake, 4.5f);
            tone(2, 74.0f, 210, WAVE_SAW);
        }
        piece->active = false;
    }
}

static void reset_combo(void) {
    combo_hits = 0;
    combo_timer = 0.0f;
    multiplier = 1;
}

static void register_hit(bool critical) {
    ++combo_hits;
    if (combo_hits > best_combo) best_combo = combo_hits;
    multiplier = 1 + clampi(combo_hits / 4, 0, 4);
    combo_timer = 2.6f;
    pulse_meter = clampf(pulse_meter + (critical ? 8.0f : 4.0f), 0.0f, 100.0f);
}

static void clear_threats(bool award_points) {
    for (int i = 0; i < MAX_THREATS; ++i) {
        if (threats[i].active) {
            float sx, sy;
            if (project_world(threats[i].x, threats[i].y, threats[i].z, &sx, &sy)) {
                add_burst(sx, sy, 12, 5, 34.0f);
            }
            threats[i].active = false;
            if (award_points) add_score(15 * multiplier);
        }
    }
}

static void game_over(void) {
    state = STATE_GAMEOVER;
    lives = 0;
    reset_combo();
    native_io_game_over(true);
    stop_music();
    tone(2, 131.0f, 500, WAVE_SAW);
    damage_flash = 0.9f;
    screen_shake = 7.0f;
}

static void damage_player(int amount) {
    if (state != STATE_PLAYING || invulnerable_timer > 0.0f) return;
    shield -= amount;
    damage_flash = 0.55f;
    screen_shake = 5.5f;
    invulnerable_timer = 0.38f;
    reset_combo();
    voice(2, 124.0f, 58.0f, 240, WAVE_SAW, 210, 0, 1, 150);
    if (shield > 0) return;

    if (god_mode()) {
        shield = MAX_SHIELD;
        invulnerable_timer = 0.65f;
        add_popup(41.0f, 53.0f, 10, "GOD ARMOR");
        voice(2, 280.0f, 760.0f, 180, WAVE_TRIANGLE, 180, 0, 4, 90);
        return;
    }

    --lives;
    if (lives <= 0) {
        game_over();
        return;
    }
    shield = MAX_SHIELD;
    invulnerable_timer = 1.6f;
    pulse_meter = clampf(pulse_meter + 20.0f, 0.0f, 100.0f);
    clear_threats(false);
    add_popup(44.0f, 53.0f, 8, "HULL LOST");
    tone(2, 196.0f, 350, WAVE_SQUARE);
}

static float enemy_radius(const Enemy *enemy) {
    float mesh_radius = 1.35f;
    if (enemy->type == ENEMY_INTERCEPTOR) mesh_radius = 1.65f;
    if (enemy->type == ENEMY_STALKER) mesh_radius = 1.55f;
    if (enemy->type == ENEMY_GUNSHIP) mesh_radius = 1.75f;
    if (enemy->type == ENEMY_BOSS) mesh_radius = 2.0f;
    float radius = (58.0f / enemy->z) * enemy->scale * mesh_radius + 1.5f;
    return clampf(radius, enemy->type == ENEMY_BOSS ? 5.0f : 3.0f,
                  enemy->type == ENEMY_BOSS ? 21.0f : 16.0f);
}

static void spawn_pickup(float x, float y, float z) {
    for (int i = 0; i < MAX_PICKUPS; ++i) {
        if (!pickups[i].active) {
            pickups[i].active = true;
            pickups[i].type = (PickupType)(random_u32() % 3u);
            pickups[i].x = x;
            pickups[i].y = y;
            pickups[i].z = z;
            pickups[i].phase = random_float(0.0f, TAU);
            return;
        }
    }
}

static void collect_pickup(int index, float sx, float sy) {
    Pickup *pickup = &pickups[index];
    const PickupType type = pickup->type;
    pickup->active = false;
    add_burst(sx, sy, type == PICKUP_SHIELD ? 11 : type == PICKUP_AMMO ? 10 : 12,
              12, 42.0f);
    add_ring(sx, sy, 7, 4);
    if (type == PICKUP_SHIELD) {
        shield = clampi(shield + 35, 0, MAX_SHIELD);
        add_popup(sx - 12.0f, sy - 8.0f, 11, "SHIELD +35");
        voice(2, 420.0f, 880.0f, 190, WAVE_SINE, 180, -40, 8, 90);
    } else if (type == PICKUP_AMMO) {
        ammo = MAG_SIZE;
        reload_timer = 0.0f;
        add_popup(sx - 10.0f, sy - 8.0f, 10, "FULL AMMO");
        voice(2, 560.0f, 760.0f, 130, WAVE_TRIANGLE, 175, 0, 2, 65);
    } else {
        pulse_meter = clampf(pulse_meter + 45.0f, 0.0f, 100.0f);
        add_popup(sx - 12.0f, sy - 8.0f, 12, "PULSE +45");
        voice(2, 760.0f, 1480.0f, 220, WAVE_TRIANGLE, 185, 42, 6, 120);
    }
    add_score(75 * multiplier);
}

static void kill_enemy(int index, bool critical) {
    Enemy *enemy = &enemies[index];
    if (!enemy->alive) return;
    float sx = aim_x;
    float sy = aim_y;
    (void)project_world(enemy->x, enemy->y, enemy->z, &sx, &sy);
    const int points = enemy->score_value * multiplier + (critical ? 50 * multiplier : 0);
    const bool boss = enemy->type == ENEMY_BOSS;
    const float pickup_x = enemy->x;
    const float pickup_y = enemy->y;
    const float pickup_z = enemy->z;
    enemy->alive = false;
    ++wave_resolved;
    ++total_kills;
    pulse_meter = clampf(pulse_meter + (boss ? 35.0f : 10.0f), 0.0f, 100.0f);
    add_score(points);
    add_score_popup(sx - 6.0f, sy - 8.0f, points, critical ? 10 : 7);
    add_burst(sx, sy, boss ? 10 : enemy->color, boss ? 28 : 12, boss ? 68.0f : 48.0f);
    add_ring(sx, sy, boss ? 8 : enemy->color, boss ? 8 : 4);
    add_ring(sx, sy, 7, boss ? 4 : 2);
    screen_shake = boss ? 9.0f : 2.8f;
    native_io_hit();
    const float burst_pitch = boss ? 82.0f :
        enemy->type == ENEMY_TANK || enemy->type == ENEMY_GUNSHIP ? 118.0f : 176.0f;
    voice(1, burst_pitch * 1.8f, burst_pitch, boss ? 520 : 180,
          boss ? WAVE_SAW : WAVE_NOISE, boss ? 235 : 185,
          enemy->x < 0.0f ? -58 : 58, 1, boss ? 310 : 110);
    voice(2, boss ? 980.0f : 720.0f, boss ? 92.0f : 340.0f,
          boss ? 430 : 105, WAVE_TRIANGLE, boss ? 180 : 120,
          enemy->x < 0.0f ? -30 : 30, 1, boss ? 260 : 55);
    if (boss) {
        clear_threats(true);
        pulse_flash = 0.8f;
    } else if ((random_u32() % 6u) == 0u || total_kills % 7 == 0) {
        spawn_pickup(pickup_x, pickup_y, pickup_z);
    }
}

static void damage_enemy(int index, int damage, bool critical) {
    Enemy *enemy = &enemies[index];
    if (!enemy->alive) return;
    enemy->hp -= damage;
    enemy->hit_flash = 0.12f;
    float sx = aim_x;
    float sy = aim_y;
    (void)project_world(enemy->x, enemy->y, enemy->z, &sx, &sy);
    add_burst(sx, sy, critical ? 10 : 7, critical ? 9 : 5, 35.0f);
    add_ring(sx, sy, critical ? 10 : 7, critical ? 3 : 2);
    native_io_hit();
    voice(1, critical ? 1260.0f : 840.0f, critical ? 720.0f : 560.0f,
          critical ? 95 : 58, WAVE_TRIANGLE, critical ? 170 : 115,
          enemy->x < 0.0f ? -48 : 48, 1, critical ? 52 : 28);
    if (critical) add_popup(sx - 6.0f, sy - 10.0f, 10, "CRIT");
    if (enemy->hp <= 0) kill_enemy(index, critical);
}

static void start_reload(void) {
    if (god_mode()) return;
    if (state != STATE_PLAYING || reload_timer > 0.0f || ammo >= MAG_SIZE) return;
    reload_timer = 0.72f;
    reload_stage = 0;
    voice(2, 250.0f, 330.0f, 80, WAVE_TRIANGLE, 110, -38, 1, 35);
}

static void update_reload(float dt) {
    if (reload_timer <= 0.0f) return;
    reload_timer -= dt;
    if (reload_stage == 0 && reload_timer <= 0.46f) {
        reload_stage = 1;
        voice(2, 360.0f, 450.0f, 70, WAVE_TRIANGLE, 105, 0, 1, 32);
    } else if (reload_stage == 1 && reload_timer <= 0.22f) {
        reload_stage = 2;
        voice(2, 520.0f, 680.0f, 70, WAVE_TRIANGLE, 115, 36, 1, 32);
    }
    if (reload_timer <= 0.0f) {
        reload_timer = 0.0f;
        ammo = MAG_SIZE;
        voice(2, 690.0f, 980.0f, 105, WAVE_TRIANGLE, 150, 0, 2, 55);
        add_popup(49.0f, 79.0f, 10, "READY");
    }
}

static float threat_radius(const Threat *threat) {
    float base = 2.5f;
    float depth = 8.0f;
    float maximum = 11.0f;
    switch (threat->type) {
        case THREAT_PLASMA: base = 3.5f; depth = 11.0f; maximum = 14.0f; break;
        case THREAT_MISSILE: base = 3.0f; depth = 10.0f; maximum = 13.0f; break;
        case THREAT_LANCE: base = 2.0f; depth = 6.0f; maximum = 9.0f; break;
        case THREAT_SHARD: base = 2.4f; depth = 7.0f; maximum = 10.0f; break;
        case THREAT_BOLT:
        default: break;
    }
    return clampf(base + depth / threat->z, 3.0f, maximum);
}

static void fire_weapon(void) {
    if (state != STATE_PLAYING || reload_timer > 0.0f) return;
    if (!god_mode() && ammo <= 0) {
        tone(0, 90.0f, 70, WAVE_SQUARE);
        start_reload();
        return;
    }

    if (!god_mode()) --ammo;
    else ammo = MAG_SIZE;
    ++shots_fired;
    muzzle_flash = 0.075f;
    native_io_fire();
    voice(0, god_mode() ? 390.0f : 310.0f, god_mode() ? 150.0f : 92.0f,
          god_mode() ? 105 : 78, WAVE_SAW, 225, -18, 1, 45);
    add_fx(FX_SPARK, aim_x, aim_y, 0.0f, 0.0f, 0.09f, 7, 1);

    int best_threat = -1;
    float best_threat_z = 1000.0f;
    for (int i = 0; i < MAX_THREATS; ++i) {
        if (!threats[i].active) continue;
        float sx, sy;
        if (!project_world(threats[i].x, threats[i].y, threats[i].z, &sx, &sy)) continue;
        const float radius = threat_radius(&threats[i]);
        const float dx = sx - aim_x;
        const float dy = sy - aim_y;
        if (dx * dx + dy * dy <= radius * radius && threats[i].z < best_threat_z) {
            best_threat = i;
            best_threat_z = threats[i].z;
        }
    }
    if (best_threat >= 0) {
        float sx, sy;
        (void)project_world(threats[best_threat].x, threats[best_threat].y,
                            threats[best_threat].z, &sx, &sy);
        threats[best_threat].active = false;
        ++shots_hit;
        register_hit(false);
        const int points = 35 * multiplier;
        add_score(points);
        add_score_popup(sx, sy - 6.0f, points, 12);
        add_burst(sx, sy, 12, 9, 42.0f);
        add_ring(sx, sy, 7, 3);
        hit_marker = 0.13f;
        tone(1, 960.0f, 65, WAVE_TRIANGLE);
        return;
    }

    int best_pickup = -1;
    float best_pickup_z = 1000.0f;
    for (int i = 0; i < MAX_PICKUPS; ++i) {
        if (!pickups[i].active) continue;
        float sx, sy;
        if (!project_world(pickups[i].x, pickups[i].y, pickups[i].z, &sx, &sy)) continue;
        const float dx = sx - aim_x;
        const float dy = sy - aim_y;
        if (dx * dx + dy * dy <= 36.0f && pickups[i].z < best_pickup_z) {
            best_pickup = i;
            best_pickup_z = pickups[i].z;
        }
    }
    if (best_pickup >= 0) {
        float sx, sy;
        (void)project_world(pickups[best_pickup].x, pickups[best_pickup].y,
                            pickups[best_pickup].z, &sx, &sy);
        ++shots_hit;
        register_hit(false);
        collect_pickup(best_pickup, sx, sy);
        hit_marker = 0.13f;
        return;
    }

    int best_enemy = -1;
    float best_enemy_z = 1000.0f;
    float best_distance_sq = 0.0f;
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].alive) continue;
        float sx, sy;
        if (!project_world(enemies[i].x, enemies[i].y, enemies[i].z, &sx, &sy)) continue;
        const float radius = enemy_radius(&enemies[i]);
        const float dx = sx - aim_x;
        const float dy = sy - aim_y;
        const float distance_sq = dx * dx + dy * dy;
        if (distance_sq <= radius * radius && enemies[i].z < best_enemy_z) {
            best_enemy = i;
            best_enemy_z = enemies[i].z;
            best_distance_sq = distance_sq;
        }
    }
    if (best_enemy >= 0) {
        const float radius = enemy_radius(&enemies[best_enemy]);
        const bool critical = best_distance_sq <= radius * radius * 0.16f;
        ++shots_hit;
        register_hit(critical);
        add_score((critical ? 25 : 10) * multiplier);
        const int damage = god_mode() ? enemies[best_enemy].hp : critical ? 2 : 1;
        damage_enemy(best_enemy, damage, critical || god_mode());
        hit_marker = 0.13f;
        return;
    }

    reset_combo();
    tone(1, 180.0f, 28, WAVE_NOISE);
    if (ammo == 0) start_reload();
}

static void activate_pulse(void) {
    if (state != STATE_PLAYING || pulse_meter < 100.0f) return;
    pulse_meter = 0.0f;
    pulse_flash = 0.9f;
    screen_shake = 6.0f;
    clear_threats(true);
    voice(2, 58.0f, 190.0f, 620, WAVE_SAW, 205, -30, 12, 360);
    voice(1, 1480.0f, 220.0f, 420, WAVE_TRIANGLE, 180, 30, 2, 260);
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].alive) continue;
        damage_enemy(i, enemies[i].type == ENEMY_BOSS ? 3 : 2, false);
    }
    add_popup(42.0f, 48.0f, 12, "PULSE WAVE");
}

static float attack_period(const Enemy *enemy) {
    float period;
    switch (enemy->type) {
        case ENEMY_SCOUT: period = 3.4f; break;
        case ENEMY_WEAVER: period = 2.8f; break;
        case ENEMY_TANK: period = 2.25f; break;
        case ENEMY_SNIPER: period = 1.75f; break;
        case ENEMY_INTERCEPTOR: period = 2.45f; break;
        case ENEMY_STALKER: period = 2.65f; break;
        case ENEMY_GUNSHIP: period = 2.15f; break;
        case ENEMY_BOSS: period = enemy->hp * 3 < enemy->max_hp ? 0.62f : 0.9f; break;
        default: period = 3.0f; break;
    }
    period -= clampf((float)wave * 0.035f, 0.0f, 0.55f);
    if (enemy->elite) period *= 0.75f;
    return clampf(period, 0.48f, 3.5f);
}

static bool spawn_threat_at(ThreatType type, float x, float y, float z,
                            int color, float speed, float drift_x,
                            float drift_y, int damage) {
    for (int i = 0; i < MAX_THREATS; ++i) {
        if (!threats[i].active) {
            threats[i] = (Threat){
                .active = true,
                .type = type,
                .x = x,
                .y = y,
                .z = z,
                .vz = speed,
                .phase = random_float(0.0f, TAU),
                .drift_x = drift_x,
                .drift_y = drift_y,
                .color = color,
                .damage = damage,
            };
            return true;
        }
    }
    return false;
}

static void enemy_attack(Enemy *enemy) {
    const float speed = clampf(6.2f + (float)wave * 0.12f, 6.2f, 9.4f);
    const int base_damage = 12 + clampi(wave / 3, 0, 10);
    if (enemy->type == ENEMY_BOSS) {
        const int pattern = ((int)(enemy->age * 1.5f)) % 3;
        if (pattern == 0) {
            (void)spawn_threat_at(THREAT_PLASMA, enemy->x - 0.85f, enemy->y,
                                  enemy->z, 8, speed - 0.45f, -0.22f, 0.0f,
                                  base_damage + 5);
            (void)spawn_threat_at(THREAT_PLASMA, enemy->x + 0.85f, enemy->y,
                                  enemy->z, 8, speed - 0.45f, 0.22f, 0.0f,
                                  base_damage + 5);
        } else if (pattern == 1) {
            for (int shot = -1; shot <= 1; ++shot) {
                (void)spawn_threat_at(THREAT_SHARD,
                                      enemy->x + (float)shot * 0.55f, enemy->y,
                                      enemy->z, 10, speed + 0.65f,
                                      (float)shot * 0.65f, fabsf((float)shot) * -0.12f,
                                      base_damage + 1);
            }
        } else {
            (void)spawn_threat_at(THREAT_MISSILE, enemy->x, enemy->y + 0.7f,
                                  enemy->z, 9, speed - 1.1f, 0.0f, 0.0f,
                                  base_damage + 10);
        }
        voice(3, 170.0f, 84.0f, 170, WAVE_SAW, 170, 0, 2, 95);
    } else {
        switch (enemy->type) {
            case ENEMY_SCOUT:
                (void)spawn_threat_at(THREAT_BOLT, enemy->x, enemy->y, enemy->z,
                                      8, speed + 0.25f, 0.0f, 0.0f, base_damage);
                break;
            case ENEMY_WEAVER:
                (void)spawn_threat_at(THREAT_PLASMA, enemy->x, enemy->y, enemy->z,
                                      14, speed - 0.45f, sinf(enemy->phase) * 0.42f,
                                      cosf(enemy->phase) * 0.22f, base_damage + 2);
                break;
            case ENEMY_TANK:
                (void)spawn_threat_at(THREAT_MISSILE, enemy->x, enemy->y, enemy->z,
                                      9, speed - 1.25f, 0.0f, 0.0f, base_damage + 8);
                break;
            case ENEMY_SNIPER:
                (void)spawn_threat_at(THREAT_LANCE, enemy->x, enemy->y, enemy->z,
                                      10, speed + 2.2f, 0.0f, 0.0f, base_damage + 5);
                break;
            case ENEMY_INTERCEPTOR:
                for (int shot = -1; shot <= 1; ++shot) {
                    (void)spawn_threat_at(THREAT_SHARD,
                                          enemy->x + (float)shot * 0.18f, enemy->y,
                                          enemy->z, 11, speed + 0.75f,
                                          (float)shot * 0.78f, 0.0f, base_damage - 2);
                }
                break;
            case ENEMY_STALKER:
                (void)spawn_threat_at(THREAT_PLASMA, enemy->x - 0.28f, enemy->y,
                                      enemy->z, 14, speed - 0.6f, -0.38f, 0.2f,
                                      base_damage + 1);
                (void)spawn_threat_at(THREAT_PLASMA, enemy->x + 0.28f, enemy->y,
                                      enemy->z, 14, speed - 0.6f, 0.38f, -0.2f,
                                      base_damage + 1);
                break;
            case ENEMY_GUNSHIP:
                (void)spawn_threat_at(THREAT_MISSILE, enemy->x - 0.45f, enemy->y,
                                      enemy->z, 9, speed - 1.0f, -0.12f, 0.0f,
                                      base_damage + 6);
                (void)spawn_threat_at(THREAT_BOLT, enemy->x + 0.45f, enemy->y,
                                      enemy->z, 8, speed + 0.3f, 0.12f, 0.0f,
                                      base_damage);
                break;
            case ENEMY_BOSS:
                break;
        }
        voice(3, enemy->type == ENEMY_SNIPER ? 920.0f : 360.0f,
              enemy->type == ENEMY_SNIPER ? 420.0f : 190.0f,
              enemy->type == ENEMY_TANK || enemy->type == ENEMY_GUNSHIP ? 150 : 82,
              enemy->type == ENEMY_WEAVER || enemy->type == ENEMY_STALKER
                  ? WAVE_SINE : WAVE_SQUARE,
              120, enemy->x < 0.0f ? -62 : 62, 1, 46);
    }
}

static EnemyType choose_enemy_type(int slot) {
    const uint32_t roll = random_u32() % 100u;
    if (wave <= 1) return roll < 65u ? ENEMY_SCOUT : ENEMY_WEAVER;
    if (wave == 2) return roll < 40u ? ENEMY_SCOUT : roll < 78u ? ENEMY_WEAVER : ENEMY_TANK;
    if (wave == 3) return roll < 25u ? ENEMY_SCOUT : roll < 52u ? ENEMY_WEAVER :
                          roll < 78u ? ENEMY_TANK : ENEMY_SNIPER;
    if ((slot + wave) % 7 == 0) return ENEMY_INTERCEPTOR;
    if (wave < 6) {
        if (roll < 18u) return ENEMY_SCOUT;
        if (roll < 40u) return ENEMY_WEAVER;
        if (roll < 63u) return ENEMY_TANK;
        if (roll < 82u) return ENEMY_SNIPER;
        return ENEMY_INTERCEPTOR;
    }
    if (roll < 14u) return ENEMY_SCOUT;
    if (roll < 28u) return ENEMY_WEAVER;
    if (roll < 44u) return ENEMY_TANK;
    if (roll < 58u) return ENEMY_SNIPER;
    if (roll < 72u) return ENEMY_INTERCEPTOR;
    if (roll < 86u) return ENEMY_STALKER;
    return ENEMY_GUNSHIP;
}

static void formation_position(int slot, float *x, float *y) {
    const int row = slot / 2;
    const float side = slot % 2 == 0 ? -1.0f : 1.0f;
    switch (wave % 4) {
        case 0:
            *x = side * (0.65f + (float)(row % 4) * 0.65f);
            *y = 0.75f - (float)(row % 3) * 0.75f;
            break;
        case 1:
            *x = -3.0f + (float)(slot % 7);
            *y = slot % 3 == 0 ? 0.8f : -0.55f;
            break;
        case 2:
            *x = side * (2.6f - (float)(row % 3) * 0.65f);
            *y = -0.8f + (float)(row % 4) * 0.5f;
            break;
        default:
            *x = random_float(-3.1f, 3.1f);
            *y = random_float(-1.15f, 1.15f);
            break;
    }
}

static void configure_enemy(Enemy *enemy, EnemyType type, float x, float y, int slot) {
    int hp = 1;
    int score_value = 100;
    int color = 12;
    int mesh = 2;
    float speed = 2.2f;
    float scale = 0.7f;
    switch (type) {
        case ENEMY_SCOUT:
            mesh = 2; hp = 1; score_value = 100; color = 12; speed = 2.35f; scale = 0.68f;
            break;
        case ENEMY_WEAVER:
            mesh = 1; hp = 1; score_value = 150; color = 14; speed = 1.85f; scale = 0.78f;
            break;
        case ENEMY_TANK:
            mesh = 0; hp = 3; score_value = 280; color = 9; speed = 1.25f; scale = 1.05f;
            break;
        case ENEMY_SNIPER:
            mesh = 4; hp = 2; score_value = 240; color = 8; speed = 1.42f; scale = 0.82f;
            break;
        case ENEMY_INTERCEPTOR:
            mesh = 5; hp = 2; score_value = 210; color = 11; speed = 2.7f; scale = 0.75f;
            break;
        case ENEMY_STALKER:
            mesh = 2; hp = 2; score_value = 260; color = 14; speed = 1.95f; scale = 0.9f;
            break;
        case ENEMY_GUNSHIP:
            mesh = 0; hp = 4; score_value = 360; color = 9; speed = 1.18f; scale = 1.12f;
            break;
        case ENEMY_BOSS:
            mesh = 3; hp = 18 + wave * 2; score_value = 2500 + wave * 150;
            color = 10; speed = 1.35f; scale = 1.7f;
            break;
    }
    const bool elite = type != ENEMY_BOSS && wave >= 6 && (random_u32() % 7u) == 0u;
    if (elite) {
        ++hp;
        score_value = score_value * 3 / 2;
        scale *= 1.12f;
        color = 10;
        speed *= 1.08f;
    }
    float entry_x = slot & 1 ? 4.8f : -4.8f;
    float entry_y = slot % 3 == 0 ? 1.6f : -1.2f;
    if (rail.cue == RAIL_BANK_LEFT || rail.cue == RAIL_EVADE_LEFT) entry_x = 6.5f;
    if (rail.cue == RAIL_BANK_RIGHT || rail.cue == RAIL_EVADE_RIGHT) entry_x = -6.5f;
    if (rail.cue == RAIL_CLIMB) {
        entry_x *= 0.35f;
        entry_y = -4.2f;
    } else if (rail.cue == RAIL_DIVE) {
        entry_x *= 0.35f;
        entry_y = 4.2f;
    }
    if (type == ENEMY_BOSS) {
        entry_x = 0.0f;
        entry_y = 0.0f;
    }
    *enemy = (Enemy){
        .alive = true,
        .type = type,
        .mesh_id = (uint8_t)mesh,
        .x = x + entry_x,
        .y = y + entry_y,
        .z = type == ENEMY_BOSS ? 24.0f : 21.0f + (float)(slot % 4) * 1.6f,
        .base_x = x,
        .base_y = y,
        .vz = speed + clampf((float)wave * 0.045f, 0.0f, 0.65f),
        .scale = scale,
        .phase = random_float(0.0f, TAU),
        .attack_timer = random_float(0.9f, 1.8f),
        .hit_flash = 0.0f,
        .hp = hp,
        .max_hp = hp,
        .score_value = score_value,
        .color = color,
        .elite = elite,
        .age = 0.0f,
        .entry_x = entry_x,
        .entry_y = entry_y,
    };
}

static bool spawn_enemy(void) {
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].alive) {
            if (wave % 5 == 0) {
                configure_enemy(&enemies[i], ENEMY_BOSS, 0.0f, 0.0f, wave_spawned);
            } else {
                float x, y;
                formation_position(wave_spawned, &x, &y);
                configure_enemy(&enemies[i], choose_enemy_type(wave_spawned), x, y, wave_spawned);
            }
            ++wave_spawned;
            return true;
        }
    }
    return false;
}

static void begin_wave(int number) {
    wave = number;
    wave_spawned = 0;
    wave_resolved = 0;
    wave_quota = wave % 5 == 0 ? 1 : clampi(6 + wave * 2, 8, 20);
    spawn_timer = wave % 5 == 0 ? 1.15f : 0.45f;
    wave_banner = wave % 5 == 0 ? 2.8f : 1.8f;
    state = STATE_PLAYING;
    ammo = MAG_SIZE;
    reload_timer = 0.0f;
    clear_threats(false);
    begin_wave_route();
    voice(2, wave % 5 == 0 ? 150.0f : 420.0f,
          wave % 5 == 0 ? 62.0f : 840.0f,
          wave % 5 == 0 ? 520 : 180,
          wave % 5 == 0 ? WAVE_SAW : WAVE_TRIANGLE,
          185, 0, 4, wave % 5 == 0 ? 320 : 90);
}

static void start_new_run(void) {
    active_mode = selected_mode;
    clear_entities();
    reset_rail_motion();
    score = 0;
    ammo = MAG_SIZE;
    lives = 3;
    shield = MAX_SHIELD;
    wave = 0;
    wave_bonus = 0;
    shots_fired = 0;
    shots_hit = 0;
    total_kills = 0;
    best_combo = 0;
    aim_x = 64.0f;
    aim_y = 64.0f;
    scroll = 0.0f;
    reload_timer = 0.0f;
    pulse_meter = 0.0f;
    invulnerable_timer = 0.0f;
    damage_flash = 0.0f;
    pulse_flash = 0.0f;
    muzzle_flash = 0.0f;
    screen_shake = 0.0f;
    camera_shake_x = 0.0f;
    camera_shake_y = 0.0f;
    music_timer = 0.0f;
    music_step = 0;
    reset_combo();
    native_io_game_over(false);
    begin_wave(1);
}

static void finish_wave(void) {
    if (state != STATE_PLAYING) return;
    const bool relocating = environment_for_wave(wave + 1) != current_environment();
    state = STATE_WAVE_CLEAR;
    wave_timer = wave % 5 == 0 ? 2.0f : relocating ? 1.8f : 1.25f;
    if (wave % 5 == 0 || relocating) {
        start_rail_cue(RAIL_COVER);
    } else {
        start_rail_cue(wave & 1 ? RAIL_BANK_RIGHT : RAIL_BANK_LEFT);
    }
    clear_threats(false);
    wave_bonus = 400 + wave * 125 + shield * 2 + ammo * 15;
    add_score(wave_bonus);
    shield = clampi(shield + 15, 0, MAX_SHIELD);
    voice(2, 620.0f, 1240.0f, 260, WAVE_TRIANGLE, 170, 42, 4, 150);
    voice(1, 420.0f, 840.0f, 210, WAVE_SINE, 145, -42, 8, 120);
}

static void update_director(float dt) {
    if (wave_spawned < wave_quota) {
        spawn_timer -= dt;
        const int max_live = wave % 5 == 0 ? 1 : clampi(3 + wave / 2, 3, 8);
        if (spawn_timer <= 0.0f && live_enemy_count() < max_live) {
            (void)spawn_enemy();
            spawn_timer = clampf(0.92f - (float)wave * 0.035f, 0.38f, 0.92f);
        }
    } else if (wave_resolved >= wave_quota && live_enemy_count() == 0) {
        finish_wave();
    }
}

static void update_enemies(float dt) {
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        Enemy *enemy = &enemies[i];
        if (!enemy->alive) continue;
        enemy->age += dt;
        enemy->phase += dt * (enemy->type == ENEMY_BOSS ? 0.9f : 1.8f);
        if (enemy->hit_flash > 0.0f) enemy->hit_flash -= dt;

        float target_x = enemy->base_x;
        float target_y = enemy->base_y;
        switch (enemy->type) {
            case ENEMY_SCOUT:
                target_x += sinf(enemy->phase * 1.4f) * 0.28f;
                target_y += cosf(enemy->phase * 1.1f) * 0.16f;
                enemy->z -= enemy->vz * dt;
                break;
            case ENEMY_WEAVER:
                target_x += sinf(enemy->phase * 1.7f) * 1.2f;
                target_y += cosf(enemy->phase * 2.3f) * 0.48f;
                enemy->z -= enemy->vz * dt;
                break;
            case ENEMY_TANK:
                target_x += sinf(enemy->phase * 0.55f) * 0.2f;
                enemy->z -= enemy->vz * dt;
                break;
            case ENEMY_SNIPER:
                target_x += sinf(enemy->phase * 0.8f) * 0.55f;
                target_y += sinf(enemy->phase * 1.5f) * 0.25f;
                enemy->z -= enemy->vz * dt;
                break;
            case ENEMY_INTERCEPTOR:
                target_x += sinf(enemy->phase * 2.6f) * 1.55f;
                target_y += cosf(enemy->phase * 1.9f) * 0.65f;
                enemy->z -= enemy->vz * dt;
                break;
            case ENEMY_STALKER:
                target_x += sinf(enemy->phase * 1.15f) * 1.05f;
                target_y += sinf(enemy->phase * 2.4f) * 0.72f;
                enemy->z -= enemy->vz * dt;
                break;
            case ENEMY_GUNSHIP:
                target_x += sinf(enemy->phase * 0.42f) * 0.38f;
                target_y += cosf(enemy->phase * 0.7f) * 0.2f;
                enemy->z -= enemy->vz * dt;
                break;
            case ENEMY_BOSS:
                if (enemy->z > 9.5f) enemy->z -= enemy->vz * dt;
                target_x = sinf(enemy->phase * 0.7f) * 1.85f;
                target_y = cosf(enemy->phase * 0.9f) * 0.48f;
                break;
        }

        float entry = clampf(1.0f - enemy->age / 1.15f, 0.0f, 1.0f);
        entry *= entry;
        enemy->x = target_x + enemy->entry_x * entry;
        enemy->y = target_y + enemy->entry_y * entry;

        if (enemy->z < 18.0f && !rail.protected) {
            enemy->attack_timer -= dt;
            if (enemy->attack_timer <= 0.0f) {
                enemy_attack(enemy);
                enemy->attack_timer = attack_period(enemy);
            }
        }

        if (enemy->z < 1.05f) {
            const int damage = enemy->type == ENEMY_TANK || enemy->type == ENEMY_GUNSHIP
                ? 45 : enemy->type == ENEMY_BOSS ? 60 : 30;
            enemy->alive = false;
            ++wave_resolved;
            if (rail.protected) {
                add_popup(48.0f, 54.0f, 10, rail.cue == RAIL_COVER ? "BLOCKED" : "DODGED");
                screen_shake = fmaxf(screen_shake, 2.2f);
            } else {
                damage_player(damage);
            }
        }
    }
}

static void update_threats(float dt) {
    for (int i = 0; i < MAX_THREATS; ++i) {
        Threat *threat = &threats[i];
        if (!threat->active) continue;
        const float old_z = threat->z;
        threat->z -= threat->vz * dt;
        threat->phase += dt * (threat->type == THREAT_MISSILE ? 13.0f : 9.0f);
        if (threat->z <= 0.9f) {
            threat->active = false;
            if (rail.protected) {
                float sx = 64.0f;
                float sy = 64.0f;
                (void)project_world(threat->x, threat->y, old_z, &sx, &sy);
                add_burst(clampf(sx, 6.0f, 122.0f), clampf(sy, 10.0f, 114.0f),
                          rail.cue == RAIL_COVER ? 6 : 12, 6, 38.0f);
                add_popup(48.0f, 54.0f, 10,
                          rail.cue == RAIL_COVER ? "COVERED" : "DODGED");
                tone(1, 520.0f, 45, WAVE_NOISE);
            } else {
                damage_player(threat->damage);
            }
            continue;
        }
        const float ratio = threat->z / old_z;
        threat->x *= ratio;
        threat->y *= ratio;
        switch (threat->type) {
            case THREAT_PLASMA:
                threat->x += (sinf(threat->phase) * 0.12f + threat->drift_x) * dt;
                threat->y += (cosf(threat->phase * 0.83f) * 0.08f + threat->drift_y) * dt;
                break;
            case THREAT_MISSILE:
                threat->x += sinf(threat->phase) * 0.075f * dt;
                threat->y += cosf(threat->phase * 0.71f) * 0.05f * dt;
                break;
            case THREAT_SHARD:
                threat->x += threat->drift_x * dt;
                threat->y += threat->drift_y * dt;
                break;
            case THREAT_BOLT:
            case THREAT_LANCE:
            default:
                break;
        }
    }
}

static void update_pickups(float dt) {
    for (int i = 0; i < MAX_PICKUPS; ++i) {
        Pickup *pickup = &pickups[i];
        if (!pickup->active) continue;
        pickup->phase += dt * 2.4f;
        pickup->z -= dt * 1.35f;
        if (pickup->z < 1.0f) pickup->active = false;
    }
}

static void update_effects(float dt) {
    for (int i = 0; i < MAX_FX; ++i) {
        Fx *effect = &effects[i];
        if (!effect->active) continue;
        effect->life -= dt;
        if (effect->life <= 0.0f) {
            effect->active = false;
            continue;
        }
        effect->x += effect->vx * dt;
        effect->y += effect->vy * dt;
        effect->vx *= 1.0f - clampf(dt * 2.0f, 0.0f, 0.9f);
        effect->vy *= 1.0f - clampf(dt * 2.0f, 0.0f, 0.9f);
    }
    for (int i = 0; i < MAX_POPUPS; ++i) {
        if (!popups[i].active) continue;
        popups[i].life -= dt;
        popups[i].y -= dt * 9.0f;
        if (popups[i].life <= 0.0f) popups[i].active = false;
    }
}

static float midi_frequency(int note) {
    return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

static void stop_music(void) {
    for (int channel = 4; channel < AUDIO_NUM_CHANNELS; ++channel) {
        audio_stop(channel);
    }
}

static void update_music(float dt) {
    if (state != STATE_PLAYING && state != STATE_TITLE) return;
    music_timer -= dt;
    if (music_timer > 0.0f) return;

    static const int8_t title_pattern[16] = {
        0, 4, 7, 11, 7, 4, 2, 7, 0, 4, 9, 12, 9, 7, 4, 2,
    };
    static const int8_t melody[6][16] = {
        {0, 7, 12, 7, 3, 10, 15, 10, 0, 7, 14, 10, 3, 7, 12, 7},
        {0, 3, 7, 10, 12, 10, 7, 3, 0, 5, 8, 12, 10, 8, 5, 3},
        {0, 2, 7, 9, 12, 14, 9, 7, 0, 4, 7, 11, 14, 11, 7, 4},
        {0, 5, 7, 10, 12, 10, 5, 3, 0, 3, 7, 8, 12, 8, 7, 3},
        {0, 3, 6, 10, 12, 15, 10, 6, 0, 6, 8, 13, 15, 13, 8, 3},
        {0, 7, 10, 14, 12, 10, 7, 5, 0, 5, 9, 12, 14, 12, 9, 5},
    };
    static const int roots[6] = {45, 41, 48, 38, 43, 40};
    static const float tempos[6] = {0.145f, 0.165f, 0.178f, 0.195f, 0.168f, 0.138f};
    const int step = music_step & 15;

    if (state == STATE_TITLE) {
        const int root = 45;
        voice(5, midi_frequency(root + 12 + title_pattern[step]),
              midi_frequency(root + 12 + title_pattern[step]),
              150, WAVE_TRIANGLE, 92, 38, 8, 90);
        if ((step & 3) == 0) {
            voice(4, midi_frequency(root - 12 + (step == 8 ? 5 : 0)),
                  midi_frequency(root - 12 + (step == 8 ? 5 : 0)),
                  520, WAVE_SINE, 100, -54, 18, 220);
            voice(6, midi_frequency(root + (step == 12 ? 7 : 0)),
                  midi_frequency(root + (step == 12 ? 7 : 0)),
                  420, WAVE_SAW, 46, 62, 35, 240);
        }
        if ((step & 3) == 2) {
            voice(7, 4800.0f, 2800.0f, 48, WAVE_NOISE, 34, 0, 1, 28);
        }
        music_timer = 0.19f;
    } else if (wave % 5 == 0) {
        static const int8_t boss_pattern[16] = {
            0, 0, 3, 0, 6, 3, 8, 6, 0, 0, 3, 10, 8, 6, 3, 1,
        };
        const int root = 34 + ((wave / 5) & 1);
        voice(5, midi_frequency(root + 12 + boss_pattern[step]),
              midi_frequency(root + 11 + boss_pattern[step]),
              125, WAVE_SAW, 86, 38, 2, 72);
        if ((step & 1) == 0) {
            voice(4, midi_frequency(root + (step & 4 ? 3 : 0)),
                  midi_frequency(root - 1 + (step & 4 ? 3 : 0)),
                  260, WAVE_SQUARE, 92, -58, 3, 130);
        }
        if ((step & 3) == 0) {
            voice(6, midi_frequency(root + 7), midi_frequency(root + 6),
                  430, WAVE_TRIANGLE, 46, 58, 28, 250);
            voice(7, 138.0f, 42.0f, 115, WAVE_SINE, 108, 0, 1, 70);
        } else if ((step & 1) != 0) {
            voice(7, 6200.0f, 3600.0f, 42, WAVE_NOISE, 42, 18, 1, 24);
        }
        music_timer = 0.125f;
    } else {
        const int environment = (int)current_environment();
        const int root = roots[environment] + ((wave / 2) % 3) * 2;
        const int lead_note = root + 12 + melody[environment][step];
        const int lead_wave = environment == ENV_ALIEN_FOREST ? WAVE_SINE :
                              environment == ENV_CRYSTAL_CAVERN ? WAVE_TRIANGLE :
                              environment == ENV_ORBITAL_RUINS ? WAVE_SAW : WAVE_SQUARE;
        voice(5, midi_frequency(lead_note),
              midi_frequency(lead_note + (environment == ENV_UNDERWATER ? -1 : 0)),
              (int)(tempos[environment] * 900.0f), lead_wave, 72,
              step & 1 ? 44 : -44, 3, (int)(tempos[environment] * 420.0f));
        if ((step & 1) == 0) {
            const int bass_offset = (step & 4) ? 7 : (step & 8) ? 5 : 0;
            voice(4, midi_frequency(root - 12 + bass_offset),
                  midi_frequency(root - 12 + bass_offset),
                  (int)(tempos[environment] * 1850.0f),
                  environment == ENV_UNDERWATER ? WAVE_SINE : WAVE_TRIANGLE,
                  94, -62, 4, (int)(tempos[environment] * 640.0f));
        }
        if ((step & 3) == 0) {
            voice(6, midi_frequency(root + (step & 8 ? 10 : 7)),
                  midi_frequency(root + (step & 8 ? 10 : 7)),
                  (int)(tempos[environment] * 2900.0f), WAVE_SAW,
                  36, 68, 28, (int)(tempos[environment] * 1300.0f));
            voice(7, 126.0f, 46.0f, 92, WAVE_SINE, 92, 0, 1, 58);
        } else if ((step & 1) != 0) {
            voice(7, 7000.0f, 4200.0f, 36, WAVE_NOISE, 32, 22, 1, 20);
        }
        music_timer = tempos[environment];
    }
    music_step = (music_step + 1) & 15;
}

static void update_common_timers(float dt) {
    if (combo_timer > 0.0f) {
        combo_timer -= dt;
        if (combo_timer <= 0.0f) reset_combo();
    }
    if (wave_banner > 0.0f) wave_banner -= dt;
    if (invulnerable_timer > 0.0f) invulnerable_timer -= dt;
    if (muzzle_flash > 0.0f) muzzle_flash -= dt;
    if (hit_marker > 0.0f) hit_marker -= dt;
    if (damage_flash > 0.0f) damage_flash -= dt;
    if (pulse_flash > 0.0f) pulse_flash -= dt;
    if (screen_shake > 0.0f) {
        screen_shake -= dt * 16.0f;
        if (screen_shake < 0.0f) screen_shake = 0.0f;
    }
}

static void update_camera_shake(uint32_t now) {
    if (screen_shake <= 0.0f) {
        camera_shake_x = 0.0f;
        camera_shake_y = 0.0f;
        return;
    }
    camera_shake_x = sinf((float)now * 0.17f) * screen_shake;
    camera_shake_y = cosf((float)now * 0.23f) * screen_shake * 0.6f;
}

static void update_aim(float dt, NativePadState pad, bool raw) {
    int adc_x, adc_y;
    if (native_io_adc_aim(&adc_x, &adc_y)) {
        aim_x = (float)adc_x;
        aim_y = (float)adc_y;
    }
    float x_axis = raw ? normalized_axis(pad.lx) : 0.0f;
    float y_axis = raw ? normalized_axis(pad.ly) : 0.0f;
    if (raw) {
        if (pad.buttons & XINPUT_GAMEPAD_DPAD_LEFT) x_axis -= 1.0f;
        if (pad.buttons & XINPUT_GAMEPAD_DPAD_RIGHT) x_axis += 1.0f;
        if (pad.buttons & XINPUT_GAMEPAD_DPAD_UP) y_axis += 1.0f;
        if (pad.buttons & XINPUT_GAMEPAD_DPAD_DOWN) y_axis -= 1.0f;
    } else {
        if (input_btn(0, 0)) x_axis -= 1.0f;
        if (input_btn(1, 0)) x_axis += 1.0f;
        if (input_btn(2, 0)) y_axis += 1.0f;
        if (input_btn(3, 0)) y_axis -= 1.0f;
    }
    aim_x += x_axis * 92.0f * dt;
    aim_y -= y_axis * 92.0f * dt;
    aim_x = clampf(aim_x, 2.0f, 125.0f);
    aim_y = clampf(aim_y, 9.0f, 114.0f);
}

static bool target_locked(void) {
    for (int i = 0; i < MAX_THREATS; ++i) {
        if (!threats[i].active) continue;
        float sx, sy;
        if (!project_world(threats[i].x, threats[i].y, threats[i].z, &sx, &sy)) continue;
        const float radius = threat_radius(&threats[i]);
        const float dx = sx - aim_x;
        const float dy = sy - aim_y;
        if (dx * dx + dy * dy <= radius * radius) return true;
    }
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].alive) continue;
        float sx, sy;
        if (!project_world(enemies[i].x, enemies[i].y, enemies[i].z, &sx, &sy)) continue;
        const float radius = enemy_radius(&enemies[i]);
        const float dx = sx - aim_x;
        const float dy = sy - aim_y;
        if (dx * dx + dy * dy <= radius * radius) return true;
    }
    return false;
}

static void fill_quad(int x0, int y0, int x1, int y1,
                      int x2, int y2, int x3, int y3, int color) {
    gfx_trifill(x0, y0, x1, y1, x2, y2, color);
    gfx_trifill(x0, y0, x2, y2, x3, y3, color);
}

static void draw_dither_band(int y0, int y1, int color, int spacing, int phase) {
    if (spacing < 2) spacing = 2;
    for (int y = y0; y <= y1; y += spacing) {
        const int offset = ((y / spacing) + phase) & 1 ? spacing / 2 : 0;
        for (int x = offset; x < 128; x += spacing) {
            gfx_pset(x, y, color);
        }
    }
}

static void draw_low_poly_rock(int cx, int cy, int radius,
                               int base_color, int light_color, int variant) {
    if (radius <= 1) {
        gfx_pset(cx, cy, light_color);
        return;
    }
    const int flip = variant & 1 ? -1 : 1;
    const int x[6] = {
        cx - radius,
        cx - radius / 3,
        cx + radius * 2 / 3,
        cx + radius,
        cx + radius / 4,
        cx - radius * 3 / 4,
    };
    const int y[6] = {
        cy - flip * radius / 3,
        cy - radius,
        cy - radius * 3 / 4,
        cy + flip * radius / 4,
        cy + radius,
        cy + radius * 2 / 3,
    };
    for (int i = 0; i < 6; ++i) {
        const int next = (i + 1) % 6;
        gfx_trifill(cx, cy, x[i], y[i], x[next], y[next],
                    (i + variant) % 3 == 0 ? light_color : base_color);
    }
    for (int i = 0; i < 6; ++i) {
        const int next = (i + 1) % 6;
        gfx_line(x[i], y[i], x[next], y[next], light_color);
    }
}

static void draw_planet(int cx, int cy, int radius, int base_color, int light_color) {
    gfx_circfill(cx, cy, radius, base_color);
    gfx_ellipsefill(cx - radius / 4, cy - radius / 5,
                    clampi(radius * 2 / 3, 1, radius),
                    clampi(radius / 2, 1, radius), light_color);
    gfx_ellipse(cx, cy + radius / 5, clampi(radius - 2, 1, radius),
                clampi(radius / 3, 1, radius), 13);
    if (radius > 7) {
        gfx_circ(cx + radius / 3, cy - radius / 4,
                 clampi(radius / 6, 1, 4), 5);
        gfx_circ(cx - radius / 4, cy + radius / 3,
                 clampi(radius / 8, 1, 3), 5);
    }
    gfx_circ(cx, cy, radius, 6);
}

static void draw_environment_backdrop(void) {
    const int shift_x = (int)(rail.project_x * 0.28f);
    const int shift_y = (int)(rail.project_y * 0.18f);
    switch (current_environment()) {
        case ENV_SPACE_CHASE:
            draw_planet(105 - shift_x, 24 - shift_y, 15, 1, 2);
            draw_dither_band(9, 110, 1, 13, 0);
            break;
        case ENV_MOUNTAIN_RUN: {
            gfx_rectfill(0, 8, 127, 114, 1);
            gfx_rectfill(0, 40 + shift_y / 3, 127, 74 + shift_y / 3, 13);
            draw_dither_band(43 + shift_y / 3, 73 + shift_y / 3, 6, 8, 1);
            draw_planet(23 - shift_x / 2, 25 - shift_y, 9, 2, 14);
            const int horizon = 72 + shift_y;
            gfx_trifill(-24 + shift_x, 112, 18 + shift_x, horizon - 20,
                        54 + shift_x, 112, 2);
            gfx_trifill(24 + shift_x / 2, 112, 67 + shift_x / 2, horizon - 30,
                        104 + shift_x / 2, 112, 4);
            gfx_trifill(73, 112, 111 - shift_x, horizon - 18,
                        151 - shift_x, 112, 5);
            gfx_rectfill(0, 104, 127, 127, 4);
            gfx_line(0, 104, 127, 104, 15);
            break;
        }
        case ENV_ALIEN_FOREST:
            gfx_rectfill(0, 8, 127, 114, 1);
            draw_dither_band(10, 76, 2, 10, 1);
            draw_planet(101 - shift_x / 2, 27 - shift_y, 12, 2, 14);
            gfx_rectfill(0, 79 + shift_y, 127, 127, 3);
            for (int x = 0; x < 128; x += 9) {
                gfx_trifill(x, 83 + shift_y, x + 5, 70 + shift_y,
                            x + 10, 83 + shift_y, x % 18 == 0 ? 11 : 3);
            }
            break;
        case ENV_UNDERWATER:
            gfx_rectfill(0, 8, 127, 114, 1);
            gfx_rectfill(0, 39 + shift_y / 2, 127, 74 + shift_y / 2, 2);
            gfx_rectfill(0, 75 + shift_y / 2, 127, 113, 3);
            draw_dither_band(18, 100, 12, 11, 0);
            for (int y = 42; y < 75; y += 6) {
                for (int x = (y & 4) ? 3 : 7; x < 128; x += 15) gfx_pset(x, y, 13);
            }
            gfx_trifill(0, 106, 22 - shift_x, 88 + shift_y, 47, 110, 3);
            gfx_trifill(34, 112, 79 + shift_x / 2, 91 + shift_y, 111, 112, 5);
            gfx_trifill(83, 112, 116 - shift_x, 84 + shift_y, 145, 112, 3);
            break;
        case ENV_CRYSTAL_CAVERN:
            gfx_rectfill(0, 8, 127, 114, 2);
            draw_dither_band(10, 112, 13, 9, 1);
            for (int x = -12; x < 140; x += 20) {
                const int jag = ((x / 20) & 1) ? 8 : 18;
                gfx_trifill(x, 8, x + 20, 8, x + 11 + shift_x / 3,
                            25 + jag + shift_y, 5);
                gfx_trifill(x, 114, x + 20, 114, x + 8 - shift_x / 3,
                            98 - jag / 2 + shift_y, 5);
            }
            break;
        case ENV_ORBITAL_RUINS:
            draw_planet(20 - shift_x / 3, 30 - shift_y, 21, 1, 13);
            draw_dither_band(10, 111, 5, 12, 1);
            gfx_trifill(91 - shift_x / 2, 8, 127, 8, 127, 39 + shift_y, 5);
            gfx_trifill(0, 93 + shift_y, 0, 114, 43 + shift_x / 2, 114, 2);
            break;
    }
}

static void draw_starfield(void) {
    const EnvironmentType environment = current_environment();
    if (environment == ENV_UNDERWATER) {
        const int travel = (int)(scroll * 2.0f);
        for (int i = 0; i < 20; ++i) {
            const int x = (i * 47 + (int)rail.project_x * (1 + i % 2)) & 127;
            const int y = 112 - ((i * 23 + travel * (1 + i % 3)) % 104);
            const int radius = i % 7 == 0 ? 2 : 1;
            gfx_circ(x, y, radius, i % 4 == 0 ? 6 : 12);
        }
        return;
    }
    if (environment == ENV_ALIEN_FOREST) {
        const int travel = (int)(scroll * 3.0f);
        for (int i = 0; i < 14; ++i) {
            const int x = (i * 37 + travel / (2 + i % 2)) & 127;
            const int y = 15 + (i * 29 + travel) % 70;
            gfx_circfill(x, y, i % 5 == 0 ? 1 : 0, i % 3 == 0 ? 10 : 11);
        }
        return;
    }

    const int count = environment == ENV_MOUNTAIN_RUN ? 20 : 38;
    const float trail = 0.28f + rail.speed * 0.55f;
    for (int i = 0; i < count; ++i) {
        float z = fmodf(2.0f + (float)i * 7.37f - scroll * 0.78f, 29.0f);
        if (z < 0.0f) z += 29.0f;
        z += 0.92f;
        const float x = (float)((i * 43) % 29 - 14) * 0.72f;
        const float y = (float)((i * 31) % 19 - 9) * 0.58f;
        const int color = i % 9 == 0 ? 12 : i % 4 == 0 ? 13 : 6;
        if (z < 12.0f || rail.speed > 1.35f) {
            wire3d_line((Vec3){x, y, z}, (Vec3){x, y, z + trail}, color);
        } else {
            float sx, sy;
            if (wire3d_project((Vec3){x, y, z}, &sx, &sy)) gfx_pset((int)sx, (int)sy, color);
        }
    }
}

static void draw_ambient_rocks(int count, int base_color, int light_color) {
    for (int i = 0; i < count; ++i) {
        float z = fmodf(3.5f + (float)i * 5.31f - scroll * 0.72f, 28.0f);
        if (z < 0.0f) z += 28.0f;
        z += 1.05f;
        const float side = i & 1 ? 1.0f : -1.0f;
        const float x = side * (4.5f + (float)(i % 3) * 1.15f);
        const float y = (float)((i * 7) % 9 - 4) * 0.62f;
        float sx, sy;
        if (!project_draw_world(x, y, z, &sx, &sy)) continue;
        const int radius = clampi((int)((58.0f / z) * (1.2f + (float)(i % 4) * 0.32f)),
                                  1, 19);
        const float radius_f = (float)radius;
        if (sx < -radius_f || sx > 127.0f + radius_f ||
            sy < -radius_f || sy > 127.0f + radius_f) continue;
        draw_low_poly_rock((int)sx, (int)sy, radius, base_color, light_color, i);
    }
}

static void draw_route_gate(float z, int panel_color, int edge_color) {
    float lox, loy, lix, liy, rix, riy, rox, roy;
    float lotx, loty, litx, lity, ritx, rity, rotx, roty;
    if (!project_draw_world(-4.6f, -1.8f, z, &lox, &loy) ||
        !project_draw_world(-3.25f, -1.45f, z, &lix, &liy) ||
        !project_draw_world(3.25f, -1.45f, z, &rix, &riy) ||
        !project_draw_world(4.6f, -1.8f, z, &rox, &roy) ||
        !project_draw_world(-4.6f, 1.8f, z, &lotx, &loty) ||
        !project_draw_world(-3.25f, 1.45f, z, &litx, &lity) ||
        !project_draw_world(3.25f, 1.45f, z, &ritx, &rity) ||
        !project_draw_world(4.6f, 1.8f, z, &rotx, &roty)) return;
    fill_quad((int)lox, (int)loy, (int)lix, (int)liy,
              (int)litx, (int)lity, (int)lotx, (int)loty, panel_color);
    fill_quad((int)rix, (int)riy, (int)rox, (int)roy,
              (int)rotx, (int)roty, (int)ritx, (int)rity, panel_color);
    gfx_line((int)lix, (int)liy, (int)litx, (int)lity, edge_color);
    gfx_line((int)rix, (int)riy, (int)ritx, (int)rity, edge_color);
    gfx_line((int)litx, (int)lity, (int)ritx, (int)rity, edge_color);
}

static void draw_forest_geometry(void) {
    for (int i = 0; i < (wave % 5 == 0 ? 6 : 8); ++i) {
        float z = fmodf(2.0f + (float)i * 3.8f - scroll * 0.62f, 27.0f);
        if (z < 0.0f) z += 27.0f;
        z += 1.0f;
        const int side = i & 1 ? 1 : -1;
        const float x = (float)side * (4.1f + (float)(i % 3) * 0.55f);
        float bx, by, tx, ty;
        if (!project_draw_world(x, -2.1f, z, &bx, &by) ||
            !project_draw_world(x, 3.2f, z, &tx, &ty)) continue;
        const int width = clampi((int)(30.0f / z), 1, 12);
        fill_quad((int)bx - width, (int)by, (int)bx + width, (int)by,
                  (int)tx + width / 2, (int)ty, (int)tx - width / 2, (int)ty,
                  i % 3 == 0 ? 5 : 3);
        const int crown = clampi(width * 2, 2, 20);
        gfx_circfill((int)tx - side * crown / 2, (int)ty + crown / 3,
                     crown, i % 4 == 0 ? 11 : 3);
        gfx_trifill((int)tx, (int)ty, (int)tx - side * crown * 2,
                    (int)ty + crown, (int)tx + side * crown,
                    (int)ty + crown * 2, i % 4 == 0 ? 10 : 11);
    }
}

static void draw_underwater_geometry(void) {
    draw_ambient_rocks(1, 3, 11);
    for (int i = 0; i < 8; ++i) {
        float z = fmodf(2.0f + (float)i * 4.2f - scroll * 0.5f, 26.0f);
        if (z < 0.0f) z += 26.0f;
        z += 1.0f;
        const float side = i & 1 ? 1.0f : -1.0f;
        float sx, sy;
        if (!project_draw_world(side * (3.9f + (float)(i % 3) * 0.5f), -2.0f,
                                z, &sx, &sy)) continue;
        const int height = clampi((int)(72.0f / z), 3, 24);
        const int width = clampi(height / 4, 1, 5);
        gfx_trifill((int)sx - width, (int)sy, (int)sx + width, (int)sy,
                    (int)sx + (i & 2 ? width * 2 : -width * 2),
                    (int)sy - height, i % 3 == 0 ? 11 : 3);
    }
}

static void draw_mountain_geometry(void) {
    for (int i = 0; i < 6; ++i) {
        float z = fmodf(2.5f + (float)i * 4.7f - scroll * 0.68f, 28.0f);
        if (z < 0.0f) z += 28.0f;
        z += 1.0f;
        const float side = i & 1 ? 1.0f : -1.0f;
        float sx, sy;
        if (!project_draw_world(side * (4.2f + (float)(i % 2) * 0.8f), -2.0f,
                                z, &sx, &sy)) continue;
        const int height = clampi((int)(82.0f / z), 3, 28);
        const int width = clampi(height / 2, 2, 16);
        gfx_trifill((int)sx - width, (int)sy, (int)sx + width, (int)sy,
                    (int)sx + (i & 2 ? width / 2 : -width / 2),
                    (int)sy - height, i % 3 == 0 ? 5 : 4);
        gfx_line((int)sx, (int)sy - height, (int)sx + width, (int)sy, 15);
        if (i % 3 == 0 && height > 6) {
            gfx_ellipsefill((int)sx, (int)sy - height / 3,
                            clampi(width / 3, 1, 4), clampi(height / 7, 1, 3), 6);
            gfx_line((int)sx, (int)sy - height / 3,
                     (int)sx - (int)side * width, (int)sy - height / 2, 8);
        }
    }
}

static void draw_crystal_geometry(void) {
    for (int i = 0; i < 7; ++i) {
        float z = fmodf(2.2f + (float)i * 4.25f - scroll * 0.61f, 28.0f);
        if (z < 0.0f) z += 28.0f;
        z += 1.0f;
        const float side = i & 1 ? 1.0f : -1.0f;
        float sx, sy;
        if (!project_draw_world(side * (4.0f + (float)(i % 3) * 0.65f), -1.8f,
                                z, &sx, &sy)) continue;
        const int height = clampi((int)(68.0f / z), 3, 26);
        const int width = clampi(height / 3, 1, 9);
        gfx_trifill((int)sx, (int)sy - height,
                    (int)sx - width, (int)sy,
                    (int)sx, (int)sy + height / 4, 2);
        gfx_trifill((int)sx, (int)sy - height,
                    (int)sx, (int)sy + height / 4,
                    (int)sx + width, (int)sy, i & 2 ? 13 : 14);
        gfx_line((int)sx, (int)sy - height, (int)sx, (int)sy, 7);
    }
}

static void draw_environment_geometry(void) {
    switch (current_environment()) {
        case ENV_SPACE_CHASE:
            draw_ambient_rocks(wave % 5 == 0 ? 2 : 3, 5, 13);
            for (int i = 0; i < 3; ++i) {
                float z = fmodf(7.0f + (float)i * 10.0f - scroll * 0.9f, 30.0f);
                if (z < 0.0f) z += 30.0f;
                draw_route_gate(z + 1.0f, 1, 12);
            }
            break;
        case ENV_MOUNTAIN_RUN:
            draw_mountain_geometry();
            break;
        case ENV_ALIEN_FOREST:
            draw_forest_geometry();
            break;
        case ENV_UNDERWATER:
            draw_underwater_geometry();
            break;
        case ENV_CRYSTAL_CAVERN:
            draw_crystal_geometry();
            wire3d_draw_corridor_path_ex(scroll,
                                         wave % 5 == 0 ? 2 : floor_colors[4],
                                         ceiling_colors[4],
                                         wave % 5 == 0 ? 5 : side_colors[4],
                                         rail.bend_x, rail.bend_y);
            break;
        case ENV_ORBITAL_RUINS:
            for (int i = 0; i < 5; ++i) {
                float z = fmodf(3.0f + (float)i * 6.2f - scroll * 0.82f, 30.0f);
                if (z < 0.0f) z += 30.0f;
                draw_route_gate(z + 1.0f, i & 1 ? 2 : 5, i & 1 ? 14 : 12);
            }
            break;
    }
}

static void draw_hazard_object(const SetPiece *piece, int x, int y, int radius,
                               bool debris) {
    const int r = clampi(radius, 1, debris ? 14 : 40);
    switch ((EnvironmentType)piece->mesh_id) {
        case ENV_SPACE_CHASE:
            draw_low_poly_rock(x, y, r, 5, debris ? 13 : 6,
                               (int)(piece->rotation * 4.0f));
            if (r > 5) {
                gfx_circ(x - r / 4, y - r / 5, part_size(r, 1, 5), 1);
                gfx_line(x - r / 2, y + r / 3, x + r / 3, y - r / 2, 13);
            }
            break;
        case ENV_MOUNTAIN_RUN:
            gfx_trifill(x - r, y + r, x + r, y + r,
                        x + (int)(sinf(piece->rotation) * (float)r / 3.0f),
                        y - r, 4);
            gfx_trifill(x - r / 2, y + r, x + r / 3, y - r,
                        x + r / 4, y + r, 5);
            gfx_line(x + r / 3, y - r, x + r, y + r, 15);
            break;
        case ENV_ALIEN_FOREST:
            gfx_ellipsefill(x, y, part_size(r, 2, 5), r, 3);
            gfx_ellipse(x, y, part_size(r, 2, 5), r, 11);
            gfx_circfill(x - r / 2, y - r / 3, part_size(r, 1, 2), 3);
            gfx_circfill(x + r / 2, y, part_size(r, 2, 5), 11);
            gfx_line(x - r / 3, y, x - r, y + r, 10);
            gfx_line(x + r / 3, y + r / 4, x + r, y + r, 10);
            break;
        case ENV_UNDERWATER:
            gfx_ellipsefill(x, y, r, part_size(r, 3, 5), 3);
            gfx_ellipse(x, y, r, part_size(r, 3, 5), 12);
            for (int spike = 0; spike < 8; ++spike) {
                const float angle = (float)spike * TAU / 8.0f + piece->rotation;
                const float radius_f = (float)r;
                const int ix = x + (int)(cosf(angle) * radius_f * 3.0f / 5.0f);
                const int iy = y + (int)(sinf(angle) * radius_f * 2.0f / 5.0f);
                const int ox = x + (int)(cosf(angle) * radius_f);
                const int oy = y + (int)(sinf(angle) * radius_f * 4.0f / 5.0f);
                gfx_line(ix, iy, ox, oy, spike & 1 ? 6 : 11);
            }
            gfx_circfill(x, y, part_size(r, 1, 4), 8);
            break;
        case ENV_CRYSTAL_CAVERN:
            gfx_trifill(x, y - r, x - r * 2 / 3, y + r * 2 / 3,
                        x, y + r, 2);
            gfx_trifill(x, y - r, x, y + r,
                        x + r * 2 / 3, y + r / 3, 13);
            gfx_trifill(x, y - r, x + r * 2 / 3, y + r / 3,
                        x + r / 4, y - r / 5, 14);
            gfx_line(x, y - r, x, y + r, 7);
            break;
        case ENV_ORBITAL_RUINS:
            gfx_rectfill(x - r / 2, y - r / 2, x + r / 2, y + r / 2, 5);
            gfx_rect(x - r / 2, y - r / 2, x + r / 2, y + r / 2, 6);
            gfx_rectfill(x - r, y - r / 3, x - r / 2, y + r / 3, 1);
            gfx_rectfill(x + r / 2, y - r / 3, x + r, y + r / 3, 1);
            gfx_line(x - r, y, x + r, y, 12);
            gfx_circ(x, y, part_size(r, 1, 4), 14);
            break;
    }
}

static void draw_capital_setpiece(const SetPiece *piece, int x, int y, int r) {
    const int width = clampi(r * 2, 4, 52);
    const int height = clampi(r / 2, 2, 16);
    gfx_ellipsefill(x, y, width, height, 1);
    gfx_trifill(x - width, y, x - width / 3, y - height * 2,
                x - width / 4, y + height, 5);
    gfx_trifill(x + width, y, x + width / 3, y - height * 2,
                x + width / 4, y + height, 5);
    gfx_ellipse(x, y, width, height, piece->color);
    gfx_rectfill(x - width / 2, y - height / 2,
                 x + width / 2, y + height / 2, 5);
    for (int engine = -1; engine <= 1; ++engine) {
        draw_engine(x + engine * width / 3, y + height,
                    clampi(height / 3, 1, 4), engine == 0 ? 12 : 9);
    }
    gfx_line(x - width * 3 / 4, y, x + width * 3 / 4, y, 6);
}

static void draw_creature_setpiece(const SetPiece *piece, int x, int y, int r) {
    const int flap = (int)(sinf(piece->rotation * 2.0f) * (float)part_size(r, 1, 3));
    gfx_ellipsefill(x, y, r, part_size(r, 1, 2), 2);
    gfx_ellipsefill(x - r, y + flap, r, part_size(r, 1, 3), 3);
    gfx_ellipsefill(x + r, y - flap, r, part_size(r, 1, 3), 3);
    gfx_ellipse(x, y, r, part_size(r, 1, 2), piece->color);
    gfx_circfill(x, y - r / 5, part_size(r, 1, 4), 14);
    gfx_pset(x, y - r / 5, 7);
    for (int tail = -1; tail <= 1; ++tail) {
        gfx_line(x + tail * r / 3, y + r / 3,
                 x + tail * r / 2 + flap, y + r * 3 / 2, piece->color);
    }
}

static void draw_structure_setpiece(const SetPiece *piece, int x, int y, int r) {
    if ((EnvironmentType)piece->mesh_id == ENV_CRYSTAL_CAVERN) {
        gfx_trifill(x, y - r * 2, x - r, y + r, x, y + r / 2, 2);
        gfx_trifill(x, y - r * 2, x, y + r / 2, x + r, y + r, 13);
        gfx_line(x, y - r * 2, x, y + r / 2, 14);
    } else {
        gfx_rectfill(x - r, y - r / 3, x + r, y + r / 3, 4);
        gfx_rect(x - r, y - r / 3, x + r, y + r / 3, 15);
        gfx_trifill(x - r, y + r / 3, x - r / 2, y - r,
                    x - r / 4, y + r / 3, 5);
        gfx_trifill(x + r, y + r / 3, x + r / 2, y - r,
                    x + r / 4, y + r / 3, 5);
        gfx_line(x - r * 3 / 4, y, x + r * 3 / 4, y, piece->color);
    }
}

static void draw_setpieces(bool foreground) {
    for (int i = 0; i < MAX_SETPIECES; ++i) {
        const SetPiece *piece = &setpieces[i];
        if (!piece->active) continue;
        const bool near_hazard = piece->type == SETPIECE_HAZARD && piece->z < 6.5f;
        if (near_hazard != foreground) continue;
        float sx, sy;
        if (!project_draw_world(piece->x, piece->y, piece->z, &sx, &sy)) continue;
        if (piece->type == SETPIECE_WINGMAN) {
            const int radius = clampi((int)(48.0f * piece->scale / piece->z), 2, 13);
            draw_interceptor_ship((int)sx, (int)sy, radius, piece->color,
                                  piece->rotation);
        } else if (piece->type == SETPIECE_HAZARD || piece->type == SETPIECE_DEBRIS) {
            const int radius = clampi((int)(58.0f * piece->scale / piece->z), 1,
                                      piece->type == SETPIECE_HAZARD ? 38 : 13);
            draw_hazard_object(piece, (int)sx, (int)sy, radius,
                               piece->type == SETPIECE_DEBRIS);
        } else {
            const int radius = clampi((int)(52.0f * piece->scale / piece->z), 3, 25);
            if (piece->type == SETPIECE_CAPITAL) {
                draw_capital_setpiece(piece, (int)sx, (int)sy, radius);
            } else if (piece->type == SETPIECE_CREATURE) {
                draw_creature_setpiece(piece, (int)sx, (int)sy, radius);
            } else {
                draw_structure_setpiece(piece, (int)sx, (int)sy, radius);
            }
        }
    }
}

static int part_size(int radius, int numerator, int denominator) {
    return clampi(radius * numerator / denominator, 1, 32);
}

static void draw_engine(int x, int y, int radius, int color) {
    const int glow = clampi(radius, 1, 5);
    gfx_circfill(x, y, glow, 1);
    gfx_circfill(x, y, clampi(glow - 1, 1, 4), color);
    gfx_pset(x, y, 7);
}

static void draw_scout_ship(int x, int y, int r, int accent, float phase) {
    const int bank = (int)(sinf(phase) * (float)part_size(r, 1, 4));
    gfx_trifill(x, y - r, x - r - bank, y + r / 2,
                x - r / 4, y + r / 3, 1);
    gfx_trifill(x, y - r, x + r - bank, y + r / 2,
                x + r / 4, y + r / 3, 1);
    gfx_trifill(x - r / 3, y, x - r, y + r / 2,
                x - r / 2, y + r * 2 / 3, 5);
    gfx_trifill(x + r / 3, y, x + r, y + r / 2,
                x + r / 2, y + r * 2 / 3, 5);
    gfx_ellipsefill(x, y, part_size(r, 1, 3), part_size(r, 4, 5), 5);
    gfx_ellipsefill(x, y - r / 4, part_size(r, 1, 4), part_size(r, 1, 3), 12);
    gfx_ellipse(x, y, part_size(r, 1, 3), part_size(r, 4, 5), accent);
    draw_engine(x - r / 4, y + r / 2, part_size(r, 1, 6), 9);
    draw_engine(x + r / 4, y + r / 2, part_size(r, 1, 6), 9);
    gfx_line(x, y - r, x, y + r / 2, accent);
}

static void draw_weaver_ship(int x, int y, int r, int accent, float phase) {
    const int flap = (int)(sinf(phase * 1.7f) * (float)part_size(r, 1, 5));
    gfx_ellipsefill(x - r / 2, y + flap / 2, part_size(r, 3, 5),
                    part_size(r, 2, 5), 2);
    gfx_ellipsefill(x + r / 2, y - flap / 2, part_size(r, 3, 5),
                    part_size(r, 2, 5), 2);
    gfx_trifill(x - r, y + flap / 2, x - r / 2, y - r / 2,
                x - r / 3, y + r / 2, 14);
    gfx_trifill(x + r, y - flap / 2, x + r / 2, y - r / 2,
                x + r / 3, y + r / 2, 14);
    gfx_ellipsefill(x, y, part_size(r, 2, 5), part_size(r, 3, 4), 3);
    gfx_ellipse(x, y, part_size(r, 2, 5), part_size(r, 3, 4), accent);
    gfx_circfill(x, y - r / 4, part_size(r, 1, 5), 14);
    gfx_pset(x - 1, y - r / 4, 7);
    gfx_pset(x + 1, y - r / 4, 7);
    gfx_line(x - r / 4, y + r / 2, x - r / 2, y + r, accent);
    gfx_line(x + r / 4, y + r / 2, x + r / 2, y + r, accent);
}

static void draw_tank_ship(int x, int y, int r, int accent) {
    const int pod_r = part_size(r, 1, 3);
    gfx_ellipsefill(x - r * 2 / 3, y + r / 6, pod_r, part_size(r, 1, 2), 4);
    gfx_ellipsefill(x + r * 2 / 3, y + r / 6, pod_r, part_size(r, 1, 2), 4);
    gfx_rectfill(x - r * 2 / 3, y - r / 2, x + r * 2 / 3, y + r / 2, 5);
    gfx_trifill(x - r * 2 / 3, y - r / 2, x, y - r,
                x + r * 2 / 3, y - r / 2, 6);
    gfx_rect(x - r * 2 / 3, y - r / 2, x + r * 2 / 3, y + r / 2, accent);
    gfx_ellipsefill(x, y - r / 5, part_size(r, 1, 3), part_size(r, 1, 4), 9);
    gfx_line(x - r / 2, y, x + r / 2, y, 4);
    gfx_line(x, y - r, x, y + r / 2, accent);
    draw_engine(x - r * 2 / 3, y + r / 2, part_size(r, 1, 5), 8);
    draw_engine(x + r * 2 / 3, y + r / 2, part_size(r, 1, 5), 8);
}

static void draw_sniper_ship(int x, int y, int r, int accent) {
    gfx_trifill(x, y - r, x - r / 3, y + r / 2,
                x + r / 3, y + r / 2, 5);
    gfx_ellipsefill(x, y - r / 5, part_size(r, 1, 4),
                    part_size(r, 1, 2), 2);
    gfx_line(x, y - r, x, y + r, accent);
    gfx_line(x - r / 2, y + r / 4, x + r / 2, y + r / 4, 6);
    gfx_circfill(x, y - r / 3, part_size(r, 1, 5), 8);
    gfx_circ(x, y - r / 3, part_size(r, 1, 4), accent);
    gfx_rectfill(x - 1, y + r / 2, x + 1, y + r, 4);
    gfx_pset(x, y - r / 3, 7);
}

static void draw_interceptor_ship(int x, int y, int r, int accent, float phase) {
    const int sweep = (int)(cosf(phase * 1.3f) * (float)part_size(r, 1, 6));
    gfx_trifill(x, y - r, x - r * 5 / 4, y + r * 2 / 3 + sweep,
                x - r / 4, y + r / 4, 3);
    gfx_trifill(x, y - r, x + r * 5 / 4, y + r * 2 / 3 - sweep,
                x + r / 4, y + r / 4, 3);
    gfx_trifill(x, y - r, x - r / 3, y + r * 3 / 4,
                x + r / 3, y + r * 3 / 4, 5);
    gfx_ellipsefill(x, y - r / 5, part_size(r, 1, 4),
                    part_size(r, 2, 5), 12);
    gfx_line(x - r, y + r / 2, x, y - r, accent);
    gfx_line(x + r, y + r / 2, x, y - r, accent);
    draw_engine(x - r / 3, y + r * 2 / 3, part_size(r, 1, 6), 11);
    draw_engine(x + r / 3, y + r * 2 / 3, part_size(r, 1, 6), 11);
}

static void draw_stalker_ship(int x, int y, int r, int accent, float phase) {
    const int sway = (int)(sinf(phase * 1.4f) * (float)part_size(r, 1, 4));
    gfx_ellipsefill(x, y, part_size(r, 4, 5), part_size(r, 1, 2), 2);
    gfx_ellipsefill(x - r / 2, y + r / 5, part_size(r, 1, 2),
                    part_size(r, 2, 5), 14);
    gfx_ellipsefill(x + r / 2, y + r / 5, part_size(r, 1, 2),
                    part_size(r, 2, 5), 14);
    gfx_ellipse(x, y, part_size(r, 4, 5), part_size(r, 1, 2), accent);
    gfx_circfill(x, y - r / 5, part_size(r, 1, 4), 3);
    gfx_circ(x, y - r / 5, part_size(r, 1, 4), 11);
    gfx_pset(x - 1, y - r / 5, 10);
    gfx_pset(x + 1, y - r / 5, 10);
    gfx_line(x - r / 2, y + r / 3, x - r + sway, y + r, accent);
    gfx_line(x, y + r / 3, x - sway, y + r, accent);
    gfx_line(x + r / 2, y + r / 3, x + r - sway, y + r, accent);
}

static void draw_gunship(int x, int y, int r, int accent) {
    gfx_ellipsefill(x, y, r, part_size(r, 1, 2), 5);
    gfx_rectfill(x - r * 3 / 4, y - r / 3, x + r * 3 / 4, y + r / 3, 4);
    gfx_trifill(x - r, y, x - r / 3, y - r * 3 / 4,
                x - r / 5, y + r / 3, 1);
    gfx_trifill(x + r, y, x + r / 3, y - r * 3 / 4,
                x + r / 5, y + r / 3, 1);
    gfx_ellipsefill(x, y - r / 4, part_size(r, 2, 5),
                    part_size(r, 1, 4), 9);
    gfx_ellipse(x, y, r, part_size(r, 1, 2), accent);
    gfx_circfill(x - r * 3 / 4, y + r / 5, part_size(r, 1, 4), 2);
    gfx_circfill(x + r * 3 / 4, y + r / 5, part_size(r, 1, 4), 2);
    gfx_line(x - r / 2, y, x + r / 2, y, 6);
    draw_engine(x - r / 2, y + r / 2, part_size(r, 1, 5), 9);
    draw_engine(x + r / 2, y + r / 2, part_size(r, 1, 5), 9);
}

static void draw_overseer(int x, int y, int r, int accent, float phase) {
    gfx_trifill(x - r / 4, y - r / 3, x - r * 3 / 2, y + r / 2,
                x - r / 3, y + r * 2 / 3, 2);
    gfx_trifill(x + r / 4, y - r / 3, x + r * 3 / 2, y + r / 2,
                x + r / 3, y + r * 2 / 3, 2);
    gfx_ellipsefill(x, y, r, part_size(r, 3, 5), 5);
    gfx_ellipse(x, y, r, part_size(r, 3, 5), accent);
    gfx_ellipsefill(x, y - r / 6, part_size(r, 1, 2),
                    part_size(r, 2, 5), 1);
    gfx_circfill(x, y - r / 6, part_size(r, 1, 3), 8);
    gfx_circ(x, y - r / 6, part_size(r, 1, 2), 10);
    gfx_circ(x, y - r / 6, part_size(r, 1, 3), 7);
    for (int pod = 0; pod < 4; ++pod) {
        const float angle = phase + (float)pod * TAU * 0.25f;
        const int px = x + (int)(cosf(angle) * (float)(r * 3 / 4));
        const int py = y + (int)(sinf(angle) * (float)(r / 2));
        gfx_line(x, y, px, py, 13);
        gfx_circfill(px, py, part_size(r, 1, 6), pod & 1 ? 9 : 2);
        gfx_circ(px, py, part_size(r, 1, 5), accent);
    }
    gfx_line(x - r * 3 / 4, y, x + r * 3 / 4, y, 6);
    draw_engine(x - r / 2, y + r / 2, part_size(r, 1, 5), 8);
    draw_engine(x + r / 2, y + r / 2, part_size(r, 1, 5), 8);
}

static void draw_enemy(const Enemy *enemy) {
    const int color = enemy->hit_flash > 0.0f ? 7 : enemy->color;
    float sx, sy;
    if (!project_draw_world(enemy->x, enemy->y, enemy->z, &sx, &sy)) return;
    const int x = (int)sx;
    const int y = (int)sy;
    const int radius = (int)enemy_radius(enemy);
    switch (enemy->type) {
        case ENEMY_SCOUT: draw_scout_ship(x, y, radius, color, enemy->phase); break;
        case ENEMY_WEAVER: draw_weaver_ship(x, y, radius, color, enemy->phase); break;
        case ENEMY_TANK: draw_tank_ship(x, y, radius, color); break;
        case ENEMY_SNIPER: draw_sniper_ship(x, y, radius, color); break;
        case ENEMY_INTERCEPTOR:
            draw_interceptor_ship(x, y, radius, color, enemy->phase);
            break;
        case ENEMY_STALKER: draw_stalker_ship(x, y, radius, color, enemy->phase); break;
        case ENEMY_GUNSHIP: draw_gunship(x, y, radius, color); break;
        case ENEMY_BOSS: draw_overseer(x, y, radius, color, enemy->phase); break;
    }
    if (enemy->elite) gfx_circ((int)sx, (int)sy, radius + 2, 10);
    if (enemy->attack_timer < 0.6f && enemy->z < 18.0f) {
        int telegraph = radius + 2 + (int)(enemy->attack_timer * 7.0f);
        if (telegraph < radius + 2) telegraph = radius + 2;
        gfx_circ((int)sx, (int)sy, telegraph, enemy->attack_timer < 0.2f ? 7 : 8);
    }
    if (enemy->max_hp > 1 && enemy->hp < enemy->max_hp && enemy->type != ENEMY_BOSS) {
        const int width = clampi(radius * 2, 8, 20);
        const int left = (int)sx - width / 2;
        const int top = (int)sy - radius - 4;
        gfx_rectfill(left, top, left + width, top + 1, 5);
        gfx_rectfill(left, top, left + width * enemy->hp / enemy->max_hp, top + 1, 8);
    }
}

static void draw_enemies(void) {
    int count = 0;
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].alive) continue;
        int position = count;
        while (position > 0 && enemies[draw_order[position - 1]].z < enemies[i].z) {
            draw_order[position] = draw_order[position - 1];
            --position;
        }
        draw_order[position] = i;
        ++count;
    }
    for (int i = 0; i < count; ++i) draw_enemy(&enemies[draw_order[i]]);
}

static void draw_pickups(void) {
    for (int i = 0; i < MAX_PICKUPS; ++i) {
        const Pickup *pickup = &pickups[i];
        if (!pickup->active) continue;
        float sx, sy;
        if (!project_draw_world(pickup->x, pickup->y, pickup->z, &sx, &sy)) continue;
        const int color = pickup->type == PICKUP_SHIELD ? 11 :
                          pickup->type == PICKUP_AMMO ? 10 : 12;
        const int radius = 4 + (int)(sinf(pickup->phase) * 1.5f);
        gfx_circfill((int)sx, (int)sy, clampi(radius - 1, 1, 6), 1);
        gfx_circ((int)sx, (int)sy, radius, color);
        gfx_circ((int)sx, (int)sy, radius + 2, 7);
        gfx_line((int)sx - radius, (int)sy, (int)sx + radius, (int)sy, color);
        gfx_line((int)sx, (int)sy - radius, (int)sx, (int)sy + radius, color);
        const char label[2] = {
            pickup->type == PICKUP_SHIELD ? 'S' : pickup->type == PICKUP_AMMO ? 'A' : 'P', '\0'
        };
        gfx_print(label, (int)sx - 2, (int)sy - 3, color);
    }
}

static void draw_threats(void) {
    for (int i = 0; i < MAX_THREATS; ++i) {
        const Threat *threat = &threats[i];
        if (!threat->active) continue;
        float sx, sy;
        if (!project_draw_world(threat->x, threat->y, threat->z, &sx, &sy)) continue;
        const int radius = (int)threat_radius(threat);
        const int pulse_color = sinf(threat->phase) > 0.0f ? 7 : threat->color;
        const int x = (int)sx;
        const int y = (int)sy;
        switch (threat->type) {
            case THREAT_BOLT:
                gfx_ellipsefill(x, y, clampi(radius / 2, 1, radius), radius, 2);
                gfx_ellipse(x, y, clampi(radius / 2, 1, radius), radius, pulse_color);
                gfx_line(x, y - radius - 3, x, y + radius + 3, threat->color);
                break;
            case THREAT_PLASMA:
                gfx_circfill(x, y, clampi(radius - 2, 1, radius), 2);
                gfx_circ(x, y, radius, pulse_color);
                gfx_circ(x, y, clampi(radius - 3, 1, radius), threat->color);
                for (int orb = 0; orb < 3; ++orb) {
                    const float angle = threat->phase + (float)orb * TAU / 3.0f;
                    gfx_pset(x + (int)(cosf(angle) * (float)(radius + 2)),
                             y + (int)(sinf(angle) * (float)(radius + 2)), 14);
                }
                break;
            case THREAT_MISSILE:
                gfx_trifill(x, y - radius, x - radius / 2, y + radius / 2,
                            x + radius / 2, y + radius / 2, 5);
                gfx_ellipsefill(x, y, clampi(radius / 3, 1, radius),
                                clampi(radius * 2 / 3, 1, radius), 4);
                gfx_line(x, y - radius, x, y + radius, pulse_color);
                gfx_line(x - 1, y + radius / 2, x - 2, y + radius + 4, 9);
                gfx_line(x + 1, y + radius / 2, x + 2, y + radius + 4, 10);
                break;
            case THREAT_LANCE:
                gfx_line(x, y - radius * 2, x, y + radius * 2, 10);
                gfx_line(x - 1, y - radius, x - 1, y + radius, 7);
                gfx_line(x + 1, y - radius, x + 1, y + radius, 8);
                gfx_circ(x, y, clampi(radius / 2, 1, radius), pulse_color);
                break;
            case THREAT_SHARD:
                gfx_trifill(x, y - radius, x - radius, y,
                            x, y + radius, 3);
                gfx_trifill(x, y - radius, x, y + radius,
                            x + radius, y, threat->color);
                gfx_line(x, y - radius, x, y + radius, pulse_color);
                break;
        }
    }
}

static void draw_effects(void) {
    for (int i = 0; i < MAX_FX; ++i) {
        const Fx *effect = &effects[i];
        if (!effect->active) continue;
        if (effect->type == FX_RING) {
            const float progress = 1.0f - effect->life / effect->max_life;
            gfx_circ((int)effect->x, (int)effect->y,
                     effect->size + (int)(progress * 14.0f), effect->color);
        } else if (effect->type == FX_DEBRIS) {
            gfx_rectfill((int)effect->x - 1, (int)effect->y - 1,
                         (int)effect->x + 1, (int)effect->y + 1, effect->color);
        } else {
            gfx_line((int)effect->x, (int)effect->y,
                     (int)(effect->x - effect->vx * 0.04f),
                     (int)(effect->y - effect->vy * 0.04f), effect->color);
        }
    }
    for (int i = 0; i < MAX_POPUPS; ++i) {
        if (!popups[i].active) continue;
        gfx_print(popups[i].text, (int)popups[i].x, (int)popups[i].y, popups[i].color);
    }
}

static void draw_vehicle_frame(void) {
    const int lean = (int)(rail.project_roll * 24.0f);
    const int lift = (int)(rail.project_y * 0.16f);
    const int accent = side_colors[(int)current_environment()];
    gfx_trifill(0, 127, 0, 107 + lift, 18 + lean, 96 + lift, 1);
    gfx_trifill(0, 127, 18 + lean, 96 + lift, 25 + lean, 127, 5);
    gfx_trifill(127, 127, 127, 107 + lift, 109 + lean, 96 + lift, 1);
    gfx_trifill(127, 127, 109 + lean, 96 + lift, 102 + lean, 127, 5);
    gfx_line(0, 107 + lift, 18 + lean, 96 + lift, accent);
    gfx_line(127, 107 + lift, 109 + lean, 96 + lift, accent);

    if (rail.cover <= 0.02f) return;
    const int cover_top = 127 - (int)(rail.cover * 40.0f);
    const int side = (int)(rail.cover * 17.0f);
    const int inner_left = 55 + (int)((1.0f - rail.cover) * 12.0f);
    const int inner_right = 127 - inner_left;
    int panel_color = 5;
    if (current_environment() == ENV_MOUNTAIN_RUN) panel_color = 4;
    if (current_environment() == ENV_ALIEN_FOREST) panel_color = 3;
    if (current_environment() == ENV_UNDERWATER) panel_color = 2;

    fill_quad(0, 127, 0, cover_top, inner_left - 7, cover_top + 5,
              inner_left, 127, panel_color);
    fill_quad(inner_right, 127, inner_right + 7, cover_top + 5,
              127, cover_top, 127, 127, panel_color);
    gfx_trifill(0, 127, inner_left - 7, cover_top + 5,
                inner_left, 127, 1);
    gfx_trifill(127, 127, inner_right + 7, cover_top + 5,
                inner_right, 127, 1);
    fill_quad(0, 18, side, 24, side, cover_top, 0, cover_top, 1);
    fill_quad(127 - side, 24, 127, 18, 127, cover_top,
              127 - side, cover_top, 1);
    gfx_line(0, cover_top, inner_left - 7, cover_top + 5, 6);
    gfx_line(inner_right + 7, cover_top + 5, 127, cover_top, 6);
    gfx_line(side, 24, side, cover_top, accent);
    gfx_line(127 - side, 24, 127 - side, cover_top, accent);
    gfx_pset(inner_left - 12, cover_top + 9, accent);
    gfx_pset(inner_right + 12, cover_top + 9, accent);
}

static void draw_world(void) {
    wire3d_set_view(rail.project_x + camera_shake_x,
                    rail.project_y + camera_shake_y,
                    rail.project_roll);
    draw_environment_backdrop();
    draw_starfield();
    draw_environment_geometry();
    draw_setpieces(false);
    draw_pickups();
    draw_enemies();
    draw_threats();
    draw_setpieces(true);
    wire3d_set_view(0.0f, 0.0f, 0.0f);
    draw_effects();
    draw_vehicle_frame();

    if (muzzle_flash > 0.0f) {
        gfx_line(0, 127, (int)aim_x - 3, (int)aim_y + 3, 10);
        gfx_line(127, 127, (int)aim_x + 3, (int)aim_y + 3, 9);
        gfx_circ((int)aim_x, (int)aim_y, 3, 7);
    }
    if (pulse_flash > 0.0f) {
        const int radius = (int)((1.0f - pulse_flash / 0.9f) * 86.0f);
        gfx_circ(64, 64, radius, 12);
        if (radius > 4) gfx_circ(64, 64, radius - 3, 7);
    }
}

static void draw_crosshair(void) {
    const int x = (int)aim_x;
    const int y = (int)aim_y;
    int color = target_locked() ? 11 : 12;
    if (ammo == 0 || reload_timer > 0.0f) color = reload_timer > 0.0f ? 10 : 8;
    const int spread = reload_timer > 0.0f ? 8 : muzzle_flash > 0.0f ? 7 : 5;
    gfx_circ(x, y, spread, color);
    gfx_line(x - spread - 5, y, x - spread - 1, y, 7);
    gfx_line(x + spread + 1, y, x + spread + 5, y, 7);
    gfx_line(x, y - spread - 5, x, y - spread - 1, 7);
    gfx_line(x, y + spread + 1, x, y + spread + 5, 7);
    gfx_pset(x, y, color);
    if (hit_marker > 0.0f) {
        gfx_line(x - 5, y - 5, x - 2, y - 2, 7);
        gfx_line(x + 2, y - 2, x + 5, y - 5, 7);
        gfx_line(x - 5, y + 5, x - 2, y + 2, 7);
        gfx_line(x + 2, y + 2, x + 5, y + 5, 7);
    }
}

static Enemy *find_boss(void) {
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].alive && enemies[i].type == ENEMY_BOSS) return &enemies[i];
    }
    return NULL;
}

static void draw_hud(void) {
    char text[48];
    gfx_rectfill(0, 0, 127, 7, 0);
    snprintf(text, sizeof(text), "%06d", score);
    gfx_print(text, 1, 1, 7);
    snprintf(text, sizeof(text), "W%02d", wave);
    gfx_print(text, 110, 1, wave % 5 == 0 ? 8 : 12);

    const int shield_color = shield > 55 ? 11 : shield > 25 ? 10 : 8;
    gfx_rectfill(1, 116, 52, 121, 0);
    gfx_rect(1, 116, 52, 121, 7);
    gfx_rectfill(3, 118, 3 + shield * 47 / MAX_SHIELD, 119, shield_color);
    gfx_print("SH", 3, 110, shield_color);

    if (god_mode()) {
        gfx_rectfill(56, 115, 91, 122, 0);
        gfx_rect(56, 115, 91, 122, 10);
        gfx_print("AMMO INF", 58, 117, 10);
    } else {
        for (int i = 0; i < MAG_SIZE; ++i) {
            const int x = 58 + i * 4;
            gfx_rectfill(x, 117, x + 2, 121, i < ammo ? 10 : 5);
        }
    }
    if (!god_mode() && reload_timer > 0.0f) {
        const int width = (int)((0.72f - reload_timer) / 0.72f * 31.0f);
        gfx_rectfill(58, 113, 89, 114, 5);
        gfx_rectfill(58, 113, 58 + width, 114, 10);
    } else if (!god_mode() && ammo == 0) {
        gfx_print("RELOAD", 60, 108, 8);
    }

    gfx_rect(94, 116, 126, 121, 7);
    gfx_rectfill(96, 118, 96 + (int)(pulse_meter * 28.0f / 100.0f), 119,
                 pulse_meter >= 100.0f ? 7 : 12);
    gfx_print(pulse_meter >= 100.0f ? "Q:PULSE" : "PULSE", 96, 110,
              pulse_meter >= 100.0f ? 10 : 12);

    if (god_mode()) {
        gfx_print("LIFE INF", 2, 123, 10);
    } else {
        for (int i = 0; i < lives; ++i) {
            const int x = 2 + i * 7;
            gfx_line(x, 126, x + 2, 123, 14);
            gfx_line(x + 2, 123, x + 4, 126, 14);
            gfx_line(x, 126, x + 4, 126, 14);
        }
    }
    if (multiplier > 1) {
        snprintf(text, sizeof(text), "X%d %d", multiplier, combo_hits);
        gfx_print(text, 101, 9, multiplier >= 4 ? 10 : 14);
    }

    Enemy *boss = find_boss();
    if (boss != NULL) {
        gfx_rectfill(22, 9, 105, 15, 0);
        gfx_rect(22, 9, 105, 15, 8);
        gfx_rectfill(24, 11, 24 + boss->hp * 79 / boss->max_hp, 13, boss->hp * 3 < boss->max_hp ? 8 : 10);
        gfx_print("OVERSEER", 48, 16, 8);
    } else if (wave_quota > 0) {
        gfx_rectfill(42, 5, 85, 6, 5);
        gfx_rectfill(42, 5, 42 + wave_resolved * 43 / wave_quota, 6, 12);
    }
}

static void draw_damage_overlay(void) {
    if (damage_flash <= 0.0f) return;
    const int thickness = damage_flash > 0.3f ? 4 : 2;
    for (int i = 0; i < thickness; ++i) gfx_rect(i, i, 127 - i, 127 - i, 8);
    for (int y = 2; y < 126; y += 4) {
        gfx_pset(2, y, 8);
        gfx_pset(125, y + 1, 8);
    }
}

static void draw_wave_banner(void) {
    if (wave_banner <= 0.0f || state != STATE_PLAYING) return;
    char text[24];
    gfx_rectfill(16, 44, 111, 82, 0);
    gfx_rect(16, 44, 111, 82, wave % 5 == 0 ? 8 : 12);
    if (wave % 5 == 0) {
        gfx_print("!! WARNING !!", 36, 49, 8);
        gfx_print("OVERSEER INBOUND", 28, 59, 10);
    } else {
        snprintf(text, sizeof(text), "SECTOR %02d", wave);
        gfx_print(text, 43, 49, 12);
        snprintf(text, sizeof(text), "%d HOSTILES", wave_quota);
        gfx_print(text, (128 - gfx_text_width(text, 4)) / 2, 69, 7);
    }
    const char *location = environment_name();
    gfx_print(location, (128 - gfx_text_width(location, 4)) / 2,
              wave % 5 == 0 ? 70 : 59, wave % 5 == 0 ? 14 : 10);
}

static void draw_rail_callout(void) {
    if (rail.callout_timer <= 0.0f ||
        (state != STATE_PLAYING && state != STATE_WAVE_CLEAR)) return;
    const int width = gfx_text_width(rail.callout, 4);
    const int x = (128 - width) / 2;
    gfx_rectfill(x - 3, 98, x + width + 2, 106, 0);
    gfx_rect(x - 3, 98, x + width + 2, 106,
             rail.protected ? 10 : 12);
    gfx_print(rail.callout, x, 100, rail.protected ? 10 : 7);
}

static void draw_title(void) {
    const float spin = title_time * 0.8f;
    wire3d_draw_mesh_rot(5, 0.0f, 0.05f, 7.5f, 1.25f, spin, 12);
    gfx_rectfill(13, 16, 114, 48, 0);
    gfx_rect(13, 16, 114, 48, 12);
    gfx_print("FRUIT JAM", 45, 22, 10);
    gfx_print("VECTOR RAID", 38, 34, 12);

    gfx_print("CHOOSE RUN MODE", 36, 55, 7);
    const bool arcade = selected_mode == RUN_MODE_ARCADE;
    gfx_rectfill(8, 64, 61, 82, arcade ? 1 : 0);
    gfx_rect(8, 64, 61, 82, arcade ? 12 : 5);
    gfx_print("ARCADE", 23, 70, arcade ? 7 : 6);
    gfx_rectfill(66, 64, 119, 82, arcade ? 0 : 2);
    gfx_rect(66, 64, 119, 82, arcade ? 5 : 10);
    gfx_print("GOD TEST", 77, 70, arcade ? 6 : 10);
    gfx_print("<  LEFT / RIGHT  >", 28, 87, 13);
    if (((int)(title_time * 2.0f) & 1) == 0) {
        gfx_print("PRESS RETURN / START", 24, 98, 7);
    }
    gfx_print(arcade ? "3 HULLS  8 SHOTS" : "1-SHOT  LIFE+AMMO INF",
              arcade ? 31 : 20, 108, arcade ? 12 : 10);
    char text[32];
    snprintf(text, sizeof(text), "HI %06d", high_score);
    gfx_print(text, 1, 119, 13);
    snprintf(text, sizeof(text), "COINS %02d", credits);
    gfx_print(text, 93, 119, 13);
}

static void draw_pause(void) {
    for (int y = 12; y < 116; y += 4) {
        for (int x = (y / 4) & 1 ? 2 : 0; x < 128; x += 4) gfx_pset(x, y, 5);
    }
    gfx_rectfill(26, 38, 101, 88, 0);
    gfx_rect(26, 38, 101, 88, 10);
    gfx_print("PAUSED", 50, 46, 10);
    gfx_print("RETURN: RESUME", 36, 60, 7);
    gfx_print("R/B: RELOAD", 40, 69, 6);
    gfx_print("Q/Y: PULSE", 42, 78, 12);
}

static void draw_wave_clear(void) {
    char text[32];
    const EnvironmentType next_environment = environment_for_wave(wave + 1);
    const bool relocating = next_environment != current_environment();
    gfx_rectfill(16, 39, 111, 89, 0);
    gfx_rect(16, 39, 111, 89, 11);
    gfx_print("ROUTE CLEAR", 42, 46, 11);
    snprintf(text, sizeof(text), "BONUS +%d", wave_bonus);
    gfx_print(text, (128 - gfx_text_width(text, 4)) / 2, 57, 10);
    const char *transition = relocating ? "MOVING TO" : "PUSHING DEEPER";
    gfx_print(transition, (128 - gfx_text_width(transition, 4)) / 2, 68, 12);
    const char *next = environment_label(next_environment);
    gfx_print(next, (128 - gfx_text_width(next, 4)) / 2, 78, 14);
}

static const char *game_rank(void) {
    const int accuracy = shots_fired > 0 ? shots_hit * 100 / shots_fired : 0;
    if (wave >= 10 && accuracy >= 70) return "S";
    if (wave >= 7 && accuracy >= 55) return "A";
    if (wave >= 5 || accuracy >= 50) return "B";
    if (wave >= 3 || accuracy >= 35) return "C";
    return "D";
}

static void draw_game_over(void) {
    char text[40];
    gfx_rectfill(14, 25, 113, 103, 0);
    gfx_rect(14, 25, 113, 103, 8);
    gfx_print("MISSION FAILED", 35, 32, 8);
    snprintf(text, sizeof(text), "SCORE %06d", score);
    gfx_print(text, 38, 45, 7);
    snprintf(text, sizeof(text), "HIGH  %06d", high_score);
    gfx_print(text, 38, 54, 13);
    const int accuracy = shots_fired > 0 ? shots_hit * 100 / shots_fired : 0;
    snprintf(text, sizeof(text), "WAVE %02d  ACC %02d%%", wave, accuracy);
    gfx_print(text, 31, 65, 12);
    snprintf(text, sizeof(text), "KILLS %d  COMBO %d", total_kills, best_combo);
    gfx_print(text, 27, 74, 14);
    snprintf(text, sizeof(text), "RANK %s", game_rank());
    gfx_print(text, 50, 84, 10);
    if (((int)(title_time * 2.0f) & 1) == 0) gfx_print("START TO RETRY", 37, 94, 7);
}

static void draw_debug(NativePadState pad, bool raw) {
    if (!debug_overlay) return;
    char text[48];
    gfx_rectfill(0, 91, 127, 109, 0);
    snprintf(text, sizeof(text), "lx%d ly%d lt%d rt%d", pad.lx, pad.ly, pad.lt, pad.rt);
    gfx_print(text, 1, 92, 11);
    snprintf(text, sizeof(text), "b%04x fps%02d e%d %s", pad.buttons, (int)fps,
             live_enemy_count(), raw ? "xinput" : "digital");
    gfx_print(text, 1, 100, 11);
}

void native_game_init(void) {
    clear_entities();
    reset_rail_motion();
    state = STATE_TITLE;
    selected_mode = RUN_MODE_ARCADE;
    active_mode = RUN_MODE_ARCADE;
    score = 0;
    high_score = 0;
    ammo = MAG_SIZE;
    lives = 3;
    shield = MAX_SHIELD;
    wave = 0;
    credits = 0;
    aim_x = 64.0f;
    aim_y = 64.0f;
    scroll = 0.0f;
    debug_overlay = false;
    rng_state = 0x8f31a6d5u;
    multiplier = 1;
    last_fps_ms = 0;
    fps_frames = 0;
    title_time = 0.0f;
    screen_shake = 0.0f;
    camera_shake_x = 0.0f;
    camera_shake_y = 0.0f;
    prev_mode_left = false;
    prev_mode_right = false;
    native_io_game_over(false);
}

void native_game_frame(float dt, uint32_t now) {
    ++fps_frames;
    if (now - last_fps_ms >= 1000u) {
        const uint32_t elapsed = now - last_fps_ms;
        fps = elapsed > 0u ? (float)fps_frames * 1000.0f / (float)elapsed : 0.0f;
        fps_frames = 0;
        last_fps_ms = now;
    }
    dt = clampf(dt, 0.001f, 0.05f);
    title_time += dt;

    const NativePadState pad = native_pad_get(0);
    const bool raw = pad.connected;
    const uint16_t buttons = pad.buttons;
    const bool fire_now = (raw && pad.rt > 50) || (raw && (buttons & XINPUT_GAMEPAD_A)) || input_btn(4, 0);
    const bool reload_now = (raw && pad.lt > 50) || (raw && (buttons & XINPUT_GAMEPAD_B)) || input_btn(5, 0);
    const bool start_now = (raw && (buttons & XINPUT_GAMEPAD_START)) || input_btn(6, 0) || native_io_start_pressed();
    const bool back_now = (raw && (buttons & XINPUT_GAMEPAD_BACK)) || native_io_service_pressed();
    const bool pulse_now = (raw && (buttons & XINPUT_GAMEPAD_Y)) || input_key(HID_KEY_Q);
    const bool coin_now = native_io_coin_pressed();
    const bool mode_left_now = (raw && (buttons & XINPUT_GAMEPAD_DPAD_LEFT)) ||
                               (!raw && input_btn(0, 0));
    const bool mode_right_now = (raw && (buttons & XINPUT_GAMEPAD_DPAD_RIGHT)) ||
                                (!raw && input_btn(1, 0));

    if (coin_now && !prev_coin) {
        credits = clampi(credits + 1, 0, 99);
        tone(2, 1047.0f, 75, WAVE_SQUARE);
    }
    if (state == STATE_TITLE) {
        if (mode_left_now && !prev_mode_left && selected_mode != RUN_MODE_ARCADE) {
            selected_mode = RUN_MODE_ARCADE;
            voice(2, 520.0f, 680.0f, 90, WAVE_TRIANGLE, 155, -45, 1, 45);
        }
        if (mode_right_now && !prev_mode_right && selected_mode != RUN_MODE_GOD_TEST) {
            selected_mode = RUN_MODE_GOD_TEST;
            voice(2, 680.0f, 1040.0f, 130, WAVE_TRIANGLE, 180, 45, 2, 65);
        }
    }
    if (start_now && !prev_start) {
        if (state == STATE_TITLE || state == STATE_GAMEOVER) {
            start_new_run();
        } else if (state == STATE_PLAYING) {
            state = STATE_PAUSED;
            stop_music();
        } else if (state == STATE_PAUSED) {
            state = STATE_PLAYING;
            music_timer = 0.0f;
        }
    }
    if (back_now && !prev_back) debug_overlay = !debug_overlay;
    update_rail_motion(dt);
    update_aim(dt, pad, raw);
    if (reload_now && !prev_reload) start_reload();
    if (pulse_now && !prev_pulse) activate_pulse();
    if (fire_now && !prev_fire) fire_weapon();

    prev_fire = fire_now;
    prev_reload = reload_now;
    prev_start = start_now;
    prev_back = back_now;
    prev_pulse = pulse_now;
    prev_coin = coin_now;
    prev_mode_left = mode_left_now;
    prev_mode_right = mode_right_now;

    update_common_timers(dt);
    update_music(dt);

    if (state == STATE_TITLE) {
        scroll += dt * 3.2f * rail.speed;
    } else if (state == STATE_PLAYING) {
        scroll += dt * (5.0f + clampf((float)wave * 0.12f, 0.0f, 2.0f)) *
                  rail.speed * environment_speed_scale();
        update_reload(dt);
        update_director(dt);
        update_enemies(dt);
        update_threats(dt);
        update_pickups(dt);
        update_setpieces(dt);
        update_effects(dt);
    } else if (state == STATE_WAVE_CLEAR) {
        scroll += dt * 3.5f * rail.speed * environment_speed_scale();
        update_setpieces(dt);
        update_effects(dt);
        wave_timer -= dt;
        if (wave_timer <= 0.0f) begin_wave(wave + 1);
    } else if (state == STATE_GAMEOVER) {
        update_effects(dt * 0.35f);
    }

    gfx_cls(0);
    draw_world();
    if (state == STATE_TITLE) {
        draw_title();
    } else {
        draw_crosshair();
        draw_hud();
        draw_wave_banner();
        if (state == STATE_PAUSED) draw_pause();
        if (state == STATE_WAVE_CLEAR) draw_wave_clear();
        if (state == STATE_GAMEOVER) draw_game_over();
        draw_rail_callout();
    }
    draw_damage_overlay();
    draw_debug(pad, raw);
    gfx_flip();
    native_io_update(now);
    update_camera_shake(now);
}
