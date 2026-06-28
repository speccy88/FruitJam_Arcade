#ifndef NATIVE_PAD_H
#define NATIVE_PAD_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NATIVE_PAD_MAX 4
typedef struct { bool connected; uint32_t frame_counter; uint16_t buttons; int16_t lx, ly, rx, ry; uint8_t lt, rt; } NativePadState;
void native_pad_init(void);
void native_pad_update_xinput(int player, uint16_t buttons, int16_t lx, int16_t ly, int16_t rx, int16_t ry, uint8_t lt, uint8_t rt);
NativePadState native_pad_get(int player);
#ifdef __cplusplus
}
#endif
#endif
