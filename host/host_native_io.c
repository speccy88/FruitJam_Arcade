#include "host_platform.h"
#include "native_io.h"

static uint32_t now_ms;
static uint32_t recoil_until;
static uint32_t muzzle_until;
static bool mouse_aim_enabled;
static bool coin_down;
static bool coin_pulse;
static bool service_pressed;
static bool game_over;
static int mouse_x = 64;
static int mouse_y = 64;

void host_io_set_mouse_aim(bool enabled, int x, int y) {
    mouse_aim_enabled = enabled;
    if (x < 0) x = 0;
    if (x > 127) x = 127;
    if (y < 0) y = 0;
    if (y > 127) y = 127;
    mouse_x = x;
    mouse_y = y;
}

void host_io_set_coin(bool pressed) {
    if (pressed && !coin_down) {
        coin_pulse = true;
    }
    coin_down = pressed;
}

void host_io_set_service(bool pressed) {
    service_pressed = pressed;
}

void native_io_init(void) {
    now_ms = 0;
    recoil_until = 0;
    muzzle_until = 0;
    mouse_aim_enabled = true;
    coin_down = false;
    coin_pulse = false;
    service_pressed = false;
    game_over = false;
}

void native_io_update(uint32_t current_ms) {
    now_ms = current_ms;
    (void)recoil_until;
    (void)muzzle_until;
    (void)game_over;
}

void native_io_fire(void) {
    recoil_until = now_ms + 45;
    muzzle_until = now_ms + 70;
}

void native_io_hit(void) {
}

void native_io_game_over(bool on) {
    game_over = on;
}

bool native_io_start_pressed(void) {
    return false;
}

bool native_io_coin_pressed(void) {
    const bool pressed = coin_pulse;
    coin_pulse = false;
    return pressed;
}

bool native_io_service_pressed(void) {
    return service_pressed;
}

bool native_io_adc_aim(int *x, int *y) {
    if (!mouse_aim_enabled) {
        return false;
    }
    *x = mouse_x;
    *y = mouse_y;
    return true;
}
