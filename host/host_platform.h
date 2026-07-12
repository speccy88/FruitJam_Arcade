#ifndef FRUITJAM_HOST_PLATFORM_H
#define FRUITJAM_HOST_PLATFORM_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
void host_input_set_button(int player, int button, bool pressed);

void host_io_set_mouse_aim(bool enabled, int x, int y);
void host_io_set_coin(bool pressed);
void host_io_set_service(bool pressed);

void host_audio_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
