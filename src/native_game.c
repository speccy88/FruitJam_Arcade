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
    ENEMY_SCOUT,
    ENEMY_WEAVER,
    ENEMY_TANK,
    ENEMY_SNIPER,
    ENEMY_INTERCEPTOR,
    ENEMY_BOSS,
} EnemyType;

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
} Enemy;

typedef struct {
    bool active;
    float x;
    float y;
    float z;
    float vz;
    float phase;
    int color;
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

static Enemy enemies[MAX_ENEMIES];
static Threat threats[MAX_THREATS];
static Pickup pickups[MAX_PICKUPS];
static Fx effects[MAX_FX];
static Popup popups[MAX_POPUPS];
static int draw_order[MAX_ENEMIES];

static GameState state;
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
static float title_time;
static bool prev_fire;
static bool prev_reload;
static bool prev_start;
static bool prev_back;
static bool prev_pulse;
static bool prev_coin;
static bool debug_overlay;
static uint32_t rng_state;
static uint32_t fps_frames;
static uint32_t last_fps_ms;
static float fps;

static const int floor_colors[] = {5, 3, 4, 2, 5, 1};
static const int ceiling_colors[] = {1, 13, 2, 5, 1, 13};
static const int side_colors[] = {13, 12, 9, 14, 11, 8};

static bool project_world(float x, float y, float z, float *sx, float *sy);

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
}

static void add_score(int points) {
    score += points;
    if (score > high_score) high_score = score;
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

static bool project_world(float x, float y, float z, float *sx, float *sy) {
    return wire3d_project((Vec3){x, y, z}, sx, sy);
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
    audio_stop(3);
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
    tone(2, 92.0f, 180, WAVE_SAW);
    if (shield > 0) return;

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
        tone(2, 740.0f, 120, WAVE_SINE);
    } else if (type == PICKUP_AMMO) {
        ammo = MAG_SIZE;
        reload_timer = 0.0f;
        add_popup(sx - 10.0f, sy - 8.0f, 10, "FULL AMMO");
        tone(2, 620.0f, 110, WAVE_TRIANGLE);
    } else {
        pulse_meter = clampf(pulse_meter + 45.0f, 0.0f, 100.0f);
        add_popup(sx - 12.0f, sy - 8.0f, 12, "PULSE +45");
        tone(2, 920.0f, 120, WAVE_TRIANGLE);
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
    tone(1, boss ? 72.0f : 135.0f, boss ? 380 : 120, boss ? WAVE_SAW : WAVE_NOISE);
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
    tone(1, critical ? 1080.0f : 820.0f, critical ? 80 : 48, WAVE_TRIANGLE);
    if (critical) add_popup(sx - 6.0f, sy - 10.0f, 10, "CRIT");
    if (enemy->hp <= 0) kill_enemy(index, critical);
}

static void start_reload(void) {
    if (state != STATE_PLAYING || reload_timer > 0.0f || ammo >= MAG_SIZE) return;
    reload_timer = 0.72f;
    reload_stage = 0;
    tone(2, 330.0f, 55, WAVE_TRIANGLE);
}

static void update_reload(float dt) {
    if (reload_timer <= 0.0f) return;
    reload_timer -= dt;
    if (reload_stage == 0 && reload_timer <= 0.46f) {
        reload_stage = 1;
        tone(2, 440.0f, 50, WAVE_TRIANGLE);
    } else if (reload_stage == 1 && reload_timer <= 0.22f) {
        reload_stage = 2;
        tone(2, 660.0f, 50, WAVE_TRIANGLE);
    }
    if (reload_timer <= 0.0f) {
        reload_timer = 0.0f;
        ammo = MAG_SIZE;
        tone(2, 880.0f, 70, WAVE_TRIANGLE);
        add_popup(49.0f, 79.0f, 10, "READY");
    }
}

static float threat_radius(const Threat *threat) {
    return clampf(2.5f + 8.0f / threat->z, 3.0f, 11.0f);
}

static void fire_weapon(void) {
    if (state != STATE_PLAYING || reload_timer > 0.0f) return;
    if (ammo <= 0) {
        tone(0, 90.0f, 70, WAVE_SQUARE);
        start_reload();
        return;
    }

    --ammo;
    ++shots_fired;
    muzzle_flash = 0.075f;
    native_io_fire();
    tone(0, 245.0f, 55, WAVE_SQUARE);
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
        damage_enemy(best_enemy, critical ? 2 : 1, critical);
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
    tone(2, 72.0f, 420, WAVE_SAW);
    tone(1, 1240.0f, 220, WAVE_TRIANGLE);
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
        case ENEMY_BOSS: period = enemy->hp * 3 < enemy->max_hp ? 0.62f : 0.9f; break;
        default: period = 3.0f; break;
    }
    period -= clampf((float)wave * 0.035f, 0.0f, 0.55f);
    if (enemy->elite) period *= 0.75f;
    return clampf(period, 0.48f, 3.5f);
}

static bool spawn_threat_at(float x, float y, float z, int color, float speed) {
    for (int i = 0; i < MAX_THREATS; ++i) {
        if (!threats[i].active) {
            threats[i] = (Threat){true, x, y, z, speed, 0.0f, color};
            tone(2, 310.0f, 42, WAVE_SQUARE);
            return true;
        }
    }
    return false;
}

static void enemy_attack(Enemy *enemy) {
    const float speed = 6.3f + (float)wave * 0.14f;
    if (enemy->type == ENEMY_BOSS) {
        (void)spawn_threat_at(enemy->x - 0.75f, enemy->y, enemy->z, 8, speed + 0.6f);
        (void)spawn_threat_at(enemy->x + 0.75f, enemy->y, enemy->z, 8, speed + 0.6f);
        if (enemy->hp * 2 < enemy->max_hp) {
            (void)spawn_threat_at(enemy->x, enemy->y + 0.6f, enemy->z, 10, speed + 1.0f);
        }
    } else {
        (void)spawn_threat_at(enemy->x, enemy->y, enemy->z,
                              enemy->type == ENEMY_SNIPER ? 10 : 8, speed);
    }
}

static EnemyType choose_enemy_type(int slot) {
    const uint32_t roll = random_u32() % 100u;
    if (wave <= 1) return roll < 65u ? ENEMY_SCOUT : ENEMY_WEAVER;
    if (wave == 2) return roll < 40u ? ENEMY_SCOUT : roll < 78u ? ENEMY_WEAVER : ENEMY_TANK;
    if (wave == 3) return roll < 25u ? ENEMY_SCOUT : roll < 52u ? ENEMY_WEAVER :
                          roll < 78u ? ENEMY_TANK : ENEMY_SNIPER;
    if ((slot + wave) % 7 == 0) return ENEMY_INTERCEPTOR;
    if (roll < 18u) return ENEMY_SCOUT;
    if (roll < 40u) return ENEMY_WEAVER;
    if (roll < 63u) return ENEMY_TANK;
    if (roll < 82u) return ENEMY_SNIPER;
    return ENEMY_INTERCEPTOR;
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
    *enemy = (Enemy){
        true, type, (uint8_t)mesh, x, y,
        type == ENEMY_BOSS ? 24.0f : 21.0f + (float)(slot % 4) * 1.6f,
        x, y, speed + clampf((float)wave * 0.045f, 0.0f, 0.65f), scale,
        random_float(0.0f, TAU), random_float(0.9f, 1.8f), 0.0f,
        hp, hp, score_value, color, elite,
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
    tone(2, wave % 5 == 0 ? 110.0f : 523.0f, wave % 5 == 0 ? 420 : 100,
         wave % 5 == 0 ? WAVE_SAW : WAVE_TRIANGLE);
}

static void start_new_run(void) {
    clear_entities();
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
    music_timer = 0.0f;
    music_step = 0;
    reset_combo();
    native_io_game_over(false);
    begin_wave(1);
}

static void finish_wave(void) {
    if (state != STATE_PLAYING) return;
    state = STATE_WAVE_CLEAR;
    wave_timer = 2.5f;
    clear_threats(false);
    wave_bonus = 400 + wave * 125 + shield * 2 + ammo * 15;
    add_score(wave_bonus);
    shield = clampi(shield + 15, 0, MAX_SHIELD);
    tone(2, 880.0f, 180, WAVE_TRIANGLE);
    tone(1, 660.0f, 120, WAVE_SINE);
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
        enemy->phase += dt * (enemy->type == ENEMY_BOSS ? 0.9f : 1.8f);
        if (enemy->hit_flash > 0.0f) enemy->hit_flash -= dt;

        switch (enemy->type) {
            case ENEMY_SCOUT:
                enemy->x = enemy->base_x + sinf(enemy->phase * 1.4f) * 0.28f;
                enemy->y = enemy->base_y + cosf(enemy->phase * 1.1f) * 0.16f;
                enemy->z -= enemy->vz * dt;
                break;
            case ENEMY_WEAVER:
                enemy->x = enemy->base_x + sinf(enemy->phase * 1.7f) * 1.2f;
                enemy->y = enemy->base_y + cosf(enemy->phase * 2.3f) * 0.48f;
                enemy->z -= enemy->vz * dt;
                break;
            case ENEMY_TANK:
                enemy->x = enemy->base_x + sinf(enemy->phase * 0.55f) * 0.2f;
                enemy->z -= enemy->vz * dt;
                break;
            case ENEMY_SNIPER:
                enemy->x = enemy->base_x + sinf(enemy->phase * 0.8f) * 0.55f;
                enemy->y = enemy->base_y + sinf(enemy->phase * 1.5f) * 0.25f;
                enemy->z -= enemy->vz * dt;
                break;
            case ENEMY_INTERCEPTOR:
                enemy->x = enemy->base_x + sinf(enemy->phase * 2.6f) * 1.55f;
                enemy->y = enemy->base_y + cosf(enemy->phase * 1.9f) * 0.65f;
                enemy->z -= enemy->vz * dt;
                break;
            case ENEMY_BOSS:
                if (enemy->z > 9.5f) enemy->z -= enemy->vz * dt;
                enemy->x = sinf(enemy->phase * 0.7f) * 1.85f;
                enemy->y = cosf(enemy->phase * 0.9f) * 0.48f;
                break;
        }

        if (enemy->z < 18.0f) {
            enemy->attack_timer -= dt;
            if (enemy->attack_timer <= 0.0f) {
                enemy_attack(enemy);
                enemy->attack_timer = attack_period(enemy);
            }
        }

        if (enemy->z < 1.05f) {
            const int damage = enemy->type == ENEMY_TANK ? 45 : 30;
            enemy->alive = false;
            ++wave_resolved;
            damage_player(damage);
        }
    }
}

static void update_threats(float dt) {
    for (int i = 0; i < MAX_THREATS; ++i) {
        Threat *threat = &threats[i];
        if (!threat->active) continue;
        const float old_z = threat->z;
        threat->z -= threat->vz * dt;
        threat->phase += dt * 9.0f;
        if (threat->z <= 0.9f) {
            threat->active = false;
            damage_player(14 + clampi(wave / 3, 0, 12));
            continue;
        }
        const float ratio = threat->z / old_z;
        threat->x *= ratio;
        threat->y *= ratio;
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

static void update_music(float dt) {
    if (state != STATE_PLAYING && state != STATE_TITLE) return;
    music_timer -= dt;
    if (music_timer > 0.0f) return;
    static const float title_notes[] = {220.0f, 277.0f, 330.0f, 440.0f, 330.0f, 277.0f};
    static const float play_notes[] = {110.0f, 165.0f, 220.0f, 165.0f, 131.0f, 196.0f, 247.0f, 196.0f};
    static const float boss_notes[] = {82.0f, 82.0f, 98.0f, 110.0f, 82.0f, 123.0f, 110.0f, 98.0f};
    if (state == STATE_TITLE) {
        tone(3, title_notes[music_step % 6], 42, WAVE_TRIANGLE);
        music_timer = 0.34f;
        music_step = (music_step + 1) % 6;
    } else if (wave % 5 == 0) {
        tone(3, boss_notes[music_step % 8], 55, WAVE_SAW);
        music_timer = 0.22f;
        music_step = (music_step + 1) % 8;
    } else {
        tone(3, play_notes[music_step % 8], 38, WAVE_TRIANGLE);
        music_timer = 0.27f;
        music_step = (music_step + 1) % 8;
    }
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

static void draw_starfield(void) {
    const int travel = (int)(scroll * 7.0f);
    for (int i = 0; i < 36; ++i) {
        const int speed = 1 + i % 3;
        const int x = (i * 53 + travel * speed) & 127;
        const int y = (i * 29 + travel / 2 + i * i) & 127;
        const int color = i % 7 == 0 ? 12 : i % 3 == 0 ? 13 : 5;
        gfx_pset(x, y, color);
        if (speed == 3 && x < 127) gfx_pset(x + 1, y, color);
    }
}

static void draw_enemy(const Enemy *enemy) {
    const int color = enemy->hit_flash > 0.0f ? 7 : enemy->color;
    wire3d_draw_mesh_rot(enemy->mesh_id, enemy->x, enemy->y, enemy->z,
                         enemy->scale, enemy->phase, color);
    float sx, sy;
    if (!project_world(enemy->x, enemy->y, enemy->z, &sx, &sy)) return;
    const int radius = (int)enemy_radius(enemy);
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
        if (!project_world(pickup->x, pickup->y, pickup->z, &sx, &sy)) continue;
        const int color = pickup->type == PICKUP_SHIELD ? 11 :
                          pickup->type == PICKUP_AMMO ? 10 : 12;
        const int radius = 4 + (int)(sinf(pickup->phase) * 1.5f);
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
        if (!project_world(threat->x, threat->y, threat->z, &sx, &sy)) continue;
        const int radius = (int)threat_radius(threat);
        const int pulse_color = sinf(threat->phase) > 0.0f ? 7 : threat->color;
        gfx_circ((int)sx, (int)sy, radius, pulse_color);
        gfx_circ((int)sx, (int)sy, radius > 3 ? radius - 2 : 1, threat->color);
        gfx_line((int)sx - radius - 2, (int)sy, (int)sx + radius + 2, (int)sy, threat->color);
        gfx_line((int)sx, (int)sy - radius - 2, (int)sx, (int)sy + radius + 2, threat->color);
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

static void draw_world(uint32_t now) {
    float shake_x = 0.0f;
    float shake_y = 0.0f;
    if (screen_shake > 0.0f) {
        shake_x = sinf((float)now * 0.17f) * screen_shake;
        shake_y = cosf((float)now * 0.23f) * screen_shake * 0.6f;
    }
    const float bank = sinf(scroll * 0.035f) * 0.018f;
    wire3d_set_view(shake_x, shake_y, bank);
    draw_starfield();
    const int theme = wave > 0 ? (wave - 1) % 6 : 0;
    wire3d_draw_corridor_ex(scroll, floor_colors[theme], ceiling_colors[theme], side_colors[theme]);
    draw_pickups();
    draw_enemies();
    draw_threats();
    wire3d_set_view(0.0f, 0.0f, 0.0f);
    draw_effects();

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

    for (int i = 0; i < MAG_SIZE; ++i) {
        const int x = 58 + i * 4;
        gfx_rectfill(x, 117, x + 2, 121, i < ammo ? 10 : 5);
    }
    if (reload_timer > 0.0f) {
        const int width = (int)((0.72f - reload_timer) / 0.72f * 31.0f);
        gfx_rectfill(58, 113, 89, 114, 5);
        gfx_rectfill(58, 113, 58 + width, 114, 10);
    } else if (ammo == 0) {
        gfx_print("RELOAD", 60, 108, 8);
    }

    gfx_rect(94, 116, 126, 121, 7);
    gfx_rectfill(96, 118, 96 + (int)(pulse_meter * 28.0f / 100.0f), 119,
                 pulse_meter >= 100.0f ? 7 : 12);
    gfx_print(pulse_meter >= 100.0f ? "Q:PULSE" : "PULSE", 96, 110,
              pulse_meter >= 100.0f ? 10 : 12);

    for (int i = 0; i < lives; ++i) {
        const int x = 2 + i * 7;
        gfx_line(x, 126, x + 2, 123, 14);
        gfx_line(x + 2, 123, x + 4, 126, 14);
        gfx_line(x, 126, x + 4, 126, 14);
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
    gfx_rectfill(22, 48, 105, 77, 0);
    gfx_rect(22, 48, 105, 77, wave % 5 == 0 ? 8 : 12);
    if (wave % 5 == 0) {
        gfx_print("!! WARNING !!", 36, 54, 8);
        gfx_print("OVERSEER INBOUND", 28, 65, 10);
    } else {
        snprintf(text, sizeof(text), "SECTOR %02d", wave);
        gfx_print(text, 43, 54, 12);
        snprintf(text, sizeof(text), "%d HOSTILES", wave_quota);
        gfx_print(text, 42, 65, 7);
    }
}

static void draw_title(void) {
    const float spin = title_time * 0.8f;
    wire3d_draw_mesh_rot(5, 0.0f, 0.05f, 7.5f, 1.25f, spin, 12);
    gfx_rectfill(16, 22, 111, 52, 0);
    gfx_rect(16, 22, 111, 52, 12);
    gfx_print("FRUIT JAM", 45, 28, 10);
    gfx_print("VECTOR RAID", 38, 39, 12);
    if (((int)(title_time * 2.0f) & 1) == 0) {
        gfx_print("PRESS RETURN / START", 24, 61, 7);
    }
    if (((int)(title_time / 3.0f) & 1) == 0) {
        gfx_print("AIM  MOUSE / STICK", 25, 76, 6);
        gfx_print("FIRE CLICK / A / Z", 27, 84, 6);
        gfx_print("RELOAD R / B", 38, 92, 6);
        gfx_print("PULSE Q / Y", 40, 100, 12);
    } else {
        gfx_print("CHAIN HITS FOR X5", 29, 78, 14);
        gfx_print("SHOOT ENEMY FIRE", 31, 87, 8);
        gfx_print("BOSS EVERY 5 WAVES", 25, 96, 10);
    }
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
    gfx_rectfill(20, 42, 107, 85, 0);
    gfx_rect(20, 42, 107, 85, 11);
    gfx_print("SECTOR CLEAR", 39, 49, 11);
    snprintf(text, sizeof(text), "BONUS +%d", wave_bonus);
    gfx_print(text, 41, 61, 10);
    gfx_print("SHIELD +15", 43, 71, 12);
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
    state = STATE_TITLE;
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

    if (coin_now && !prev_coin) {
        credits = clampi(credits + 1, 0, 99);
        tone(2, 1047.0f, 75, WAVE_SQUARE);
    }
    if (start_now && !prev_start) {
        if (state == STATE_TITLE || state == STATE_GAMEOVER) {
            start_new_run();
        } else if (state == STATE_PLAYING) {
            state = STATE_PAUSED;
            audio_stop(3);
        } else if (state == STATE_PAUSED) {
            state = STATE_PLAYING;
            music_timer = 0.0f;
        }
    }
    if (back_now && !prev_back) debug_overlay = !debug_overlay;
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

    update_common_timers(dt);
    update_music(dt);

    if (state == STATE_TITLE) {
        scroll += dt * 3.2f;
    } else if (state == STATE_PLAYING) {
        scroll += dt * (5.0f + clampf((float)wave * 0.12f, 0.0f, 2.0f));
        update_reload(dt);
        update_director(dt);
        update_enemies(dt);
        update_threats(dt);
        update_pickups(dt);
        update_effects(dt);
    } else if (state == STATE_WAVE_CLEAR) {
        scroll += dt * 2.0f;
        update_effects(dt);
        wave_timer -= dt;
        if (wave_timer <= 0.0f) begin_wave(wave + 1);
    } else if (state == STATE_GAMEOVER) {
        update_effects(dt * 0.35f);
    }

    gfx_cls(0);
    draw_world(now);
    if (state == STATE_TITLE) {
        draw_title();
    } else {
        draw_crosshair();
        draw_hud();
        draw_wave_banner();
        if (state == STATE_PAUSED) draw_pause();
        if (state == STATE_WAVE_CLEAR) draw_wave_clear();
        if (state == STATE_GAMEOVER) draw_game_over();
    }
    draw_damage_overlay();
    draw_debug(pad, raw);
    gfx_flip();
    native_io_update(now);
}
