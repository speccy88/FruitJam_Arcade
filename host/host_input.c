#include "host_platform.h"
#include "input.h"

#include <string.h>

static bool buttons[2][7];
static uint8_t button_holds[2][7];
static bool keys[256];
static input_modifier_poll_fn modifier_poll;
static input_mouse_poll_fn mouse_poll;
static int mouse_x = 64;
static int mouse_y = 64;
static int mouse_wheel;
static uint8_t mouse_buttons;

void host_input_set_button(int player, int button, bool pressed) {
    if (player < 0 || player >= 2 || button < 0 || button >= 7) {
        return;
    }
    buttons[player][button] = pressed;
}

void input_key_callback(uint8_t keycode, char ascii, bool pressed, uint8_t modifiers) {
    (void)ascii;
    (void)modifiers;
    keys[keycode] = pressed;
}

void input_update(void) {
    if (modifier_poll) {
        (void)modifier_poll();
    }
    if (mouse_poll) {
        mouse_poll();
    }
    for (int player = 0; player < 2; ++player) {
        for (int button = 0; button < 7; ++button) {
            if (buttons[player][button]) {
                if (button_holds[player][button] < UINT8_MAX) {
                    ++button_holds[player][button];
                }
            } else {
                button_holds[player][button] = 0;
            }
        }
    }
}

bool input_btn(int button, int player) {
    if (player < 0 || player >= 2 || button < 0 || button >= 7) {
        return false;
    }
    return buttons[player][button];
}

bool input_btnp(int button, int player) {
    if (!input_btn(button, player)) {
        return false;
    }
    const uint8_t held = button_holds[player][button];
    return held == 1 || (held >= 16 && ((held - 16) % 4) == 0);
}

bool input_key(uint8_t keycode) {
    return keys[keycode];
}

void input_set_modifier_poll(input_modifier_poll_fn fn) {
    modifier_poll = fn;
}

int input_getchar(void) {
    return -1;
}

void input_flush(void) {
}

void input_set_mouse_poll(input_mouse_poll_fn fn) {
    mouse_poll = fn;
}

void input_mouse_update(int32_t dx, int32_t dy, int32_t wheel, uint8_t pressed) {
    mouse_x += (int)dx;
    mouse_y += (int)dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x > 127) mouse_x = 127;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y > 127) mouse_y = 127;
    mouse_wheel += (int)wheel;
    mouse_buttons = pressed;
}

int input_mouse_x(void) {
    return mouse_x;
}

int input_mouse_y(void) {
    return mouse_y;
}

uint8_t input_mouse_buttons(void) {
    return mouse_buttons;
}

int input_mouse_wheel(void) {
    const int wheel = mouse_wheel;
    mouse_wheel = 0;
    return wheel;
}

void input_mouse_reset(void) {
    mouse_x = 64;
    mouse_y = 64;
    mouse_wheel = 0;
    mouse_buttons = 0;
}

void input_gamepad_report(const uint8_t *report, uint16_t len, int player) {
    (void)report;
    (void)len;
    (void)player;
}

void input_dualsense_report(const uint8_t *report, uint16_t len, int player, uint16_t pid) {
    (void)report;
    (void)len;
    (void)player;
    (void)pid;
}

void input_xinput_update(uint16_t buttons_word, int16_t lx, int16_t ly, int player) {
    (void)buttons_word;
    (void)lx;
    (void)ly;
    (void)player;
}
