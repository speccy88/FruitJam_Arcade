#ifndef DVI_H
#define DVI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize HSTX DVI output for 640x480@60Hz.
// framebuffer: pointer to the 384x384 RGB565 graphics buffer.
// The display will continuously scan out this buffer via DMA,
// centered in the 640x480 active area.
void dvi_init(uint16_t *framebuffer);

#ifdef __cplusplus
}
#endif

#endif // DVI_H
