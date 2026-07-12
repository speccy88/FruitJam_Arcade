#ifndef NATIVE_GAME_H
#define NATIVE_GAME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void native_game_init(void);
void native_game_frame(float dt, uint32_t now_ms);
int native_game_live_enemies(void);

/* Lightweight telemetry used by the host simulator's automated demo mode. */
int native_game_score(void);
int native_game_wave(void);
int native_game_state(void);
bool native_game_demo_target(int *x, int *y);

#ifdef __cplusplus
}
#endif

#endif
