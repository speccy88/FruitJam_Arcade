#ifndef GFX_H
#define GFX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_LOGICAL_WIDTH  128
#define GFX_LOGICAL_HEIGHT 128

#ifndef GFX_SCALE
#define GFX_SCALE 1
#endif

#if GFX_SCALE < 1
#error "GFX_SCALE must be a positive integer"
#endif

#if GFX_SCALE > 8
#error "GFX_SCALE above 8 is not supported"
#endif

// Physical framebuffer dimensions. Drawing coordinates remain in the
// 128x128 logical game space regardless of render scale.
#define GFX_WIDTH  (GFX_LOGICAL_WIDTH * GFX_SCALE)
#define GFX_HEIGHT (GFX_LOGICAL_HEIGHT * GFX_SCALE)

#if GFX_WIDTH % 2
#error "GFX_WIDTH must be even for the packed 4-bit framebuffer"
#endif

// Initialize graphics subsystem (clears framebuffer, sets up palette).
// Call before dvi_init().
void gfx_init(void);

// Returns the GFX_WIDTH x GFX_HEIGHT RGB565 display framebuffer.
uint16_t *gfx_get_dvi_buffer(void);

// Drawing API (logical coordinates clipped to 0..127, colors masked to 0..15)
void gfx_cls(int color);
void gfx_pset(int x, int y, int c);
int  gfx_pget(int x, int y);
void gfx_rect(int x0, int y0, int x1, int y1, int c);
void gfx_rectfill(int x0, int y0, int x1, int y1, int c);
void gfx_line(int x0, int y0, int x1, int y1, int c);
void gfx_trifill(int x0, int y0, int x1, int y1, int x2, int y2, int c);
void gfx_circ(int cx, int cy, int r, int c);
void gfx_circfill(int cx, int cy, int r, int c);
void gfx_ellipse(int cx, int cy, int rx, int ry, int c);
void gfx_ellipsefill(int cx, int cy, int rx, int ry, int c);
void gfx_print(const char *str, int x, int y, int c);
void gfx_print_w(const char *str, int x, int y, int c, int char_w);
int  gfx_text_width(const char *str, int char_w);
void gfx_flip(void);

// Redirect the packed 4-bit framebuffer to an external
// GFX_WIDTH * GFX_HEIGHT / 2-byte buffer.
// Pass NULL to restore the default internal buffer.
void gfx_set_fb(uint8_t *buf);
uint8_t *gfx_get_fb(void);

// Set display palette for gfx_flip(). Maps framebuffer color index to display color.
// Pass NULL to disable (identity mapping). The pointer must remain valid.
void gfx_set_display_pal(const uint8_t *pal);

#ifdef __cplusplus
}
#endif

#endif // GFX_H
