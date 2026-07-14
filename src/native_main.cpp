#include <cstdarg>
#include <cstdint>
#include <cstdio>

#include "pico/stdlib.h"

#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pio_usb.h"
#include "tusb.h"
#include "usb-host/fwUSBHost.h"

extern "C" {
#include "audio.h"
#include "dvi.h"
#include "gfx.h"
#include "input.h"
#include "native_game.h"
#include "native_io.h"
#include "native_pad.h"
}

extern fwUSBHost obUSBHost;

extern "C" int cdc_debug_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int result = std::vprintf(format, args);
    va_end(args);
    return result;
}

extern "C" int tusb_debug_buffered_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int result = std::vprintf(format, args);
    va_end(args);
    return result;
}

int main() {
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    sleep_ms(10);
    set_sys_clock_pll(1260000000, 5, 1);

    gpio_init(11);
    gpio_set_dir(11, GPIO_OUT);
    gpio_put(11, 1);

    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = 1;
    pio_cfg.tx_ch = 9;
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    stdio_init_all();
    gfx_init();
    dvi_init(gfx_get_dvi_buffer());

    const bool audio_ok = audio_init();
    if (audio_ok) audio_volume(5);

    native_pad_init();
    native_io_init();
    native_game_init();

    obUSBHost.m_obHID.getKeyboard().setKeyCallback(input_key_callback);

    input_set_modifier_poll([]() -> uint8_t {
        return obUSBHost.m_obHID.getKeyboard().getModifiers();
    });
    input_set_mouse_poll([]() {
        auto &mouse = obUSBHost.m_obHID.getMouse();
        if (!mouse.isAnyMounted()) return;

        int32_t dx;
        int32_t dy;
        int32_t wheel;
        mouse.getDeltas(&dx, &dy, &wheel);
        input_mouse_update(dx, dy, wheel, mouse.getButtons());
    });
    obUSBHost.m_obHID.getController().setReportCallback(
        [](uint8_t device, uint8_t instance, const uint8_t *report,
           uint16_t length) {
            auto &controller = obUSBHost.m_obHID.getController();
            int player = controller.getPlayerForDevice(device, instance);
            if (player > 1) player = 1;

            uint16_t vendor_id = 0;
            uint16_t product_id = 0;
            tuh_vid_pid_get(device, &vendor_id, &product_id);
            if (vendor_id == SONY_VID) {
                input_dualsense_report(report, length, player, product_id);
            } else {
                input_gamepad_report(report, length, player);
            }
        });
    obUSBHost.m_obXInput.setReportCallback(
        [](uint8_t device, uint8_t instance, const xinput_gamepad_t *gamepad) {
            int player = obUSBHost.m_obXInput.getPlayerForDevice(device, instance);
            if (player > 1) player = 1;

            input_xinput_update(gamepad->wButtons, gamepad->sThumbLX,
                                gamepad->sThumbLY, player);
            native_pad_update_xinput(
                player, gamepad->wButtons, gamepad->sThumbLX,
                gamepad->sThumbLY, gamepad->sThumbRX, gamepad->sThumbRY,
                gamepad->bLeftTrigger, gamepad->bRightTrigger);
        });

    std::printf("fruitjam_railshooter native @ %u MHz\n",
                static_cast<unsigned>(clock_get_hz(clk_sys) / 1000000));

    const absolute_time_t scan_end = make_timeout_time_ms(3000);
    while (!time_reached(scan_end)) {
        tuh_task();
        input_update();
        sleep_ms(1);
    }

    absolute_time_t next = get_absolute_time();
    uint32_t last = to_ms_since_boot(next);
    while (true) {
        tuh_task();
        input_update();

        const uint32_t now = to_ms_since_boot(get_absolute_time());
        float dt = (float)(now - last) / 1000.0f;
        if (dt < 0.001f) dt = 0.001f;
        if (dt > 0.05f) dt = 0.05f;
        last = now;

        native_game_frame(dt, now);
        next = delayed_by_us(next, 16667);
        sleep_until(next);
        if (absolute_time_diff_us(get_absolute_time(), next) < -20000) {
            next = get_absolute_time();
        }
    }
}
