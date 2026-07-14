#include "native_pad.h"

static NativePadState pads[NATIVE_PAD_MAX];

void native_pad_init(void) {
    for (int i = 0; i < NATIVE_PAD_MAX; ++i) {
        pads[i] = (NativePadState){0};
    }
}

void native_pad_update_xinput(int player, uint16_t buttons, int16_t lx,
                              int16_t ly, int16_t rx, int16_t ry,
                              uint8_t lt, uint8_t rt) {
    if (player < 0 || player >= NATIVE_PAD_MAX) return;

    NativePadState *pad = &pads[player];
    pad->connected = true;
    ++pad->frame_counter;
    pad->buttons = buttons;
    pad->lx = lx;
    pad->ly = ly;
    pad->rx = rx;
    pad->ry = ry;
    pad->lt = lt;
    pad->rt = rt;
}

NativePadState native_pad_get(int player) {
    if (player < 0 || player >= NATIVE_PAD_MAX) {
        const NativePadState empty = {0};
        return empty;
    }
    return pads[player];
}
