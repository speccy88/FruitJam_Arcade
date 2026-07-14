#ifndef NATIVE_IO_H
#define NATIVE_IO_H

#include <stdbool.h>
#include <stdint.h>

#ifndef ENABLE_ARCADE_IO
#define ENABLE_ARCADE_IO 1
#endif

#ifndef ENABLE_ADC_AIM
#define ENABLE_ADC_AIM 0
#endif

#define RECOIL_GPIO 6
#define MUZZLE_GPIO 7
#define SERVICE_GPIO 10
#define START_BUTTON_GPIO 4
#define COIN_BUTTON_GPIO 5
#define STATUS_LED_GPIO 29
#define ADC_PAN_GPIO 40
#define ADC_TILT_GPIO 41

#ifdef __cplusplus
extern "C" {
#endif

void native_io_init(void);
void native_io_update(uint32_t now_ms);
void native_io_fire(void);
void native_io_hit(void);
void native_io_game_over(bool on);
bool native_io_start_pressed(void);
bool native_io_coin_pressed(void);
bool native_io_service_pressed(void);
bool native_io_adc_aim(int *x, int *y);

#ifdef __cplusplus
}
#endif

#endif
