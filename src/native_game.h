#ifndef NATIVE_GAME_H
#define NATIVE_GAME_H
#include <stdint.h>
void native_game_init(void); void native_game_frame(float dt, uint32_t now_ms); int native_game_live_enemies(void);
#endif
