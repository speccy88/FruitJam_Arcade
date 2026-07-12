#include "native_io.h"

#include "pico/stdlib.h"

#if ENABLE_ADC_AIM
#include "hardware/adc.h"
#endif

static uint32_t recoil_until;
static uint32_t muzzle_until;
static uint32_t hit_until;
static bool game_over_blink;

#if ENABLE_ARCADE_IO
static bool active_low_pressed(uint gpio) {
    return !gpio_get(gpio);
}
#endif

void native_io_init(void) {
#if ENABLE_ARCADE_IO
    const uint outputs[] = {RECOIL_GPIO, MUZZLE_GPIO, STATUS_LED_GPIO};
    for (uint i = 0; i < sizeof(outputs) / sizeof(outputs[0]); ++i) {
        gpio_init(outputs[i]);
        gpio_set_dir(outputs[i], GPIO_OUT);
    }
    gpio_put(RECOIL_GPIO, 0);
    gpio_put(MUZZLE_GPIO, 0);
    gpio_put(STATUS_LED_GPIO, 1);

    const uint inputs[] = {SERVICE_GPIO, START_BUTTON_GPIO, COIN_BUTTON_GPIO};
    for (uint i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        gpio_init(inputs[i]);
        gpio_set_dir(inputs[i], GPIO_IN);
        gpio_pull_up(inputs[i]);
    }
#endif

#if ENABLE_ADC_AIM
    adc_init();
    adc_gpio_init(ADC_PAN_GPIO);
    adc_gpio_init(ADC_TILT_GPIO);
#endif
}

void native_io_update(uint32_t now_ms) {
#if ENABLE_ARCADE_IO
    gpio_put(RECOIL_GPIO, now_ms < recoil_until);
    gpio_put(MUZZLE_GPIO, now_ms < muzzle_until);
    if (game_over_blink) {
        gpio_put(STATUS_LED_GPIO, ((now_ms / 250u) & 1u) != 0u);
    } else {
        gpio_put(STATUS_LED_GPIO, now_ms < hit_until ? 0 : 1);
    }
#else
    (void)now_ms;
#endif
}

void native_io_fire(void) {
#if ENABLE_ARCADE_IO
    const uint32_t now = to_ms_since_boot(get_absolute_time());
    recoil_until = now + 45u;
    muzzle_until = now + 70u;
#endif
}

void native_io_hit(void) {
#if ENABLE_ARCADE_IO
    const uint32_t now = to_ms_since_boot(get_absolute_time());
    hit_until = now + 65u;
    gpio_put(STATUS_LED_GPIO, 0);
#endif
}

void native_io_game_over(bool on) {
    game_over_blink = on;
#if ENABLE_ARCADE_IO
    if (!on) gpio_put(STATUS_LED_GPIO, 1);
#endif
}

bool native_io_start_pressed(void) {
#if ENABLE_ARCADE_IO
    return active_low_pressed(START_BUTTON_GPIO);
#else
    return false;
#endif
}

bool native_io_coin_pressed(void) {
#if ENABLE_ARCADE_IO
    return active_low_pressed(COIN_BUTTON_GPIO);
#else
    return false;
#endif
}

bool native_io_service_pressed(void) {
#if ENABLE_ARCADE_IO
    return active_low_pressed(SERVICE_GPIO);
#else
    return false;
#endif
}

bool native_io_adc_aim(int *x, int *y) {
#if ENABLE_ADC_AIM
    const int pan_min = 200;
    const int pan_max = 3900;
    const int tilt_min = 200;
    const int tilt_max = 3900;
    adc_select_input(0);
    uint16_t pan = adc_read();
    adc_select_input(1);
    uint16_t tilt = adc_read();
    if (pan < pan_min) pan = pan_min;
    if (pan > pan_max) pan = pan_max;
    if (tilt < tilt_min) tilt = tilt_min;
    if (tilt > tilt_max) tilt = tilt_max;
    *x = (pan - pan_min) * 127 / (pan_max - pan_min);
    *y = (tilt - tilt_min) * 127 / (tilt_max - tilt_min);
    return true;
#else
    (void)x;
    (void)y;
    return false;
#endif
}
