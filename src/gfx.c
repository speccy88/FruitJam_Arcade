/* Scalable 128x128 logical indexed-color graphics for Fruit Jam arcade. */

#include "gfx.h"
#include <string.h>

#ifndef GFX_SKIP_RGB565
#define GFX_SKIP_RGB565 0
#endif

// --- Framebuffers ---

// 4-bit indexed framebuffer: 2 physical pixels per byte.
// Low nibble = left pixel (even x), high nibble = right pixel (odd x).
#define FB4_SIZE (GFX_WIDTH * GFX_HEIGHT / 2)
static uint8_t fb4_default[FB4_SIZE];
static uint8_t *fb4 = fb4_default;

// RGB565 is needed by Fruit Jam DVI. The host reads the indexed buffer
// directly, so its target omits this duplicate full-frame output.
#if GFX_SKIP_RGB565
static uint16_t fb_dvi[1];
#else
static uint16_t fb_dvi[GFX_WIDTH * GFX_HEIGHT];
#endif

// Display palette: if set, maps framebuffer color → display color before palette lookup
static const uint8_t *gfx_disp_pal = NULL;

// --- 16-color native RGB565 palette for DVI scanout ---
// Standard RGB888 -> RGB565: R>>3 << 11 | G>>2 << 5 | B>>3

#if !GFX_SKIP_RGB565
static const uint16_t palette[16] = {
    0x0022, // 0: #050713 ink
    0x10C6, // 1: #101B35 deep navy
    0x4109, // 2: #40204A plum
    0x0A48, // 3: #0F4A42 deep teal
    0x7205, // 4: #75402C rust
    0x4A8C, // 5: #4B5366 gunmetal
    0xA577, // 6: #A4ADBD silver
    0xFFBD, // 7: #FFF7E8 warm white
    0xF9EB, // 8: #FF3F5F warning red
    0xFCE5, // 9: #FF9F2D amber
    0xFF2D, // 10: #FFE66D yellow
    0x46D3, // 11: #40D99C green
    0x3DBF, // 12: #3DB4FF cyan blue
    0x83B7, // 13: #8174B8 violet
    0xEBD6, // 14: #ED78B5 rose
    0xFE14, // 15: #FFC0A0 peach
};
#endif

// --- 4x6 bitmap font (printable ASCII 32..126) ---
// Each character is 4 pixels wide, 6 pixels tall.
// Stored as 6 bytes per char, each byte has 4 low bits = pixels (MSB=left).

// Compact 4x6 ASCII font. Low-order bits are rendered left to right.
static const uint8_t font_4x6[][6] = {
    {0x00,0x00,0x00,0x00,0x00,0x00}, // 32: space
    {0x02,0x02,0x02,0x00,0x02,0x00}, // 33: !
    {0x05,0x05,0x00,0x00,0x00,0x00}, // 34: "
    {0x05,0x07,0x05,0x07,0x05,0x00}, // 35: #
    {0x07,0x03,0x06,0x07,0x02,0x00}, // 36: $
    {0x05,0x04,0x02,0x01,0x05,0x00}, // 37: %
    {0x03,0x03,0x06,0x05,0x07,0x00}, // 38: &
    {0x02,0x01,0x00,0x00,0x00,0x00}, // 39: '
    {0x02,0x01,0x01,0x01,0x02,0x00}, // 40: (
    {0x02,0x04,0x04,0x04,0x02,0x00}, // 41: )
    {0x05,0x02,0x07,0x02,0x05,0x00}, // 42: *
    {0x00,0x02,0x07,0x02,0x00,0x00}, // 43: +
    {0x00,0x00,0x00,0x02,0x01,0x00}, // 44: ,
    {0x00,0x00,0x07,0x00,0x00,0x00}, // 45: -
    {0x00,0x00,0x00,0x00,0x02,0x00}, // 46: .
    {0x04,0x02,0x02,0x02,0x01,0x00}, // 47: /
    {0x07,0x05,0x05,0x05,0x07,0x00}, // 48: 0
    {0x03,0x02,0x02,0x02,0x07,0x00}, // 49: 1
    {0x07,0x04,0x07,0x01,0x07,0x00}, // 50: 2
    {0x07,0x04,0x06,0x04,0x07,0x00}, // 51: 3
    {0x05,0x05,0x07,0x04,0x04,0x00}, // 52: 4
    {0x07,0x01,0x07,0x04,0x07,0x00}, // 53: 5
    {0x01,0x01,0x07,0x05,0x07,0x00}, // 54: 6
    {0x07,0x04,0x04,0x04,0x04,0x00}, // 55: 7
    {0x07,0x05,0x07,0x05,0x07,0x00}, // 56: 8
    {0x07,0x05,0x07,0x04,0x04,0x00}, // 57: 9
    {0x00,0x02,0x00,0x02,0x00,0x00}, // 58: :
    {0x00,0x02,0x00,0x02,0x01,0x00}, // 59: ;
    {0x04,0x02,0x01,0x02,0x04,0x00}, // 60: <
    {0x00,0x07,0x00,0x07,0x00,0x00}, // 61: =
    {0x01,0x02,0x04,0x02,0x01,0x00}, // 62: >
    {0x07,0x04,0x06,0x00,0x02,0x00}, // 63: ?
    {0x02,0x05,0x05,0x01,0x06,0x00}, // 64: @
    {0x00,0x06,0x05,0x07,0x05,0x00}, // 65: A
    {0x00,0x03,0x03,0x05,0x07,0x00}, // 66: B
    {0x00,0x06,0x01,0x01,0x06,0x00}, // 67: C
    {0x00,0x03,0x05,0x05,0x03,0x00}, // 68: D
    {0x00,0x07,0x03,0x01,0x06,0x00}, // 69: E
    {0x00,0x07,0x03,0x01,0x01,0x00}, // 70: F
    {0x00,0x06,0x01,0x05,0x07,0x00}, // 71: G
    {0x00,0x05,0x05,0x07,0x05,0x00}, // 72: H
    {0x00,0x07,0x02,0x02,0x07,0x00}, // 73: I
    {0x00,0x07,0x02,0x02,0x03,0x00}, // 74: J
    {0x00,0x05,0x03,0x05,0x05,0x00}, // 75: K
    {0x00,0x01,0x01,0x01,0x06,0x00}, // 76: L
    {0x00,0x07,0x07,0x05,0x05,0x00}, // 77: M
    {0x00,0x03,0x05,0x05,0x05,0x00}, // 78: N
    {0x00,0x06,0x05,0x05,0x03,0x00}, // 79: O
    {0x00,0x06,0x05,0x07,0x01,0x00}, // 80: P
    {0x00,0x02,0x05,0x03,0x06,0x00}, // 81: Q
    {0x00,0x03,0x05,0x03,0x05,0x00}, // 82: R
    {0x00,0x06,0x01,0x04,0x03,0x00}, // 83: S
    {0x00,0x07,0x02,0x02,0x02,0x00}, // 84: T
    {0x00,0x05,0x05,0x05,0x06,0x00}, // 85: U
    {0x00,0x05,0x05,0x07,0x02,0x00}, // 86: V
    {0x00,0x05,0x05,0x07,0x07,0x00}, // 87: W
    {0x00,0x05,0x02,0x02,0x05,0x00}, // 88: X
    {0x00,0x05,0x07,0x04,0x03,0x00}, // 89: Y
    {0x00,0x07,0x04,0x01,0x07,0x00}, // 90: Z
    {0x03,0x01,0x01,0x01,0x03,0x00}, // 91: [
    {0x01,0x02,0x02,0x02,0x04,0x00}, // 92: backslash
    {0x06,0x04,0x04,0x04,0x06,0x00}, // 93: ]
    {0x02,0x05,0x00,0x00,0x00,0x00}, // 94: ^
    {0x00,0x00,0x00,0x00,0x07,0x00}, // 95: _
    {0x02,0x04,0x00,0x00,0x00,0x00}, // 96: `
    {0x07,0x05,0x07,0x05,0x05,0x00}, // 97: a
    {0x07,0x05,0x03,0x05,0x07,0x00}, // 98: b
    {0x06,0x01,0x01,0x01,0x06,0x00}, // 99: c
    {0x03,0x05,0x05,0x05,0x07,0x00}, // 100: d
    {0x07,0x01,0x03,0x01,0x07,0x00}, // 101: e
    {0x07,0x01,0x03,0x01,0x01,0x00}, // 102: f
    {0x06,0x01,0x01,0x05,0x07,0x00}, // 103: g
    {0x05,0x05,0x07,0x05,0x05,0x00}, // 104: h
    {0x07,0x02,0x02,0x02,0x07,0x00}, // 105: i
    {0x07,0x02,0x02,0x02,0x03,0x00}, // 106: j
    {0x05,0x05,0x03,0x05,0x05,0x00}, // 107: k
    {0x01,0x01,0x01,0x01,0x07,0x00}, // 108: l
    {0x07,0x07,0x05,0x05,0x05,0x00}, // 109: m
    {0x03,0x05,0x05,0x05,0x05,0x00}, // 110: n
    {0x06,0x05,0x05,0x05,0x03,0x00}, // 111: o
    {0x07,0x05,0x07,0x01,0x01,0x00}, // 112: p
    {0x02,0x05,0x05,0x03,0x06,0x00}, // 113: q
    {0x07,0x05,0x03,0x05,0x05,0x00}, // 114: r
    {0x06,0x01,0x07,0x04,0x03,0x00}, // 115: s
    {0x07,0x02,0x02,0x02,0x02,0x00}, // 116: t
    {0x05,0x05,0x05,0x05,0x06,0x00}, // 117: u
    {0x05,0x05,0x05,0x07,0x02,0x00}, // 118: v
    {0x05,0x05,0x05,0x07,0x07,0x00}, // 119: w
    {0x05,0x05,0x02,0x05,0x05,0x00}, // 120: x
    {0x05,0x05,0x07,0x04,0x07,0x00}, // 121: y
    {0x07,0x04,0x02,0x01,0x07,0x00}, // 122: z
    {0x06,0x02,0x03,0x02,0x06,0x00}, // 123: {
    {0x02,0x02,0x02,0x02,0x02,0x00}, // 124: |
    {0x03,0x02,0x06,0x02,0x03,0x00}, // 125: }
    {0x00,0x04,0x07,0x01,0x00,0x00}, // 126: ~
};

// --- Internal helpers ---

static inline void pset_physical_fast(int x, int y, uint8_t c) {
    int idx = y * (GFX_WIDTH / 2) + (x >> 1);
    if (x & 1)
        fb4[idx] = (fb4[idx] & 0x0F) | (c << 4);
    else
        fb4[idx] = (fb4[idx] & 0xF0) | c;
}

static inline uint8_t pget_physical_fast(int x, int y) {
    int idx = y * (GFX_WIDTH / 2) + (x >> 1);
    if (x & 1)
        return (fb4[idx] >> 4) & 0x0F;
    else
        return fb4[idx] & 0x0F;
}

static inline int clip(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline int in_physical_bounds(int x, int y) {
    return (unsigned)x < GFX_WIDTH && (unsigned)y < GFX_HEIGHT;
}

static inline int in_logical_bounds(int x, int y) {
    return (unsigned)x < GFX_LOGICAL_WIDTH && (unsigned)y < GFX_LOGICAL_HEIGHT;
}

// Logical point coordinates map to the nearest physical pixel center of their
// scaled logical cell. Filled pixels use the whole cell instead.
static inline int point_to_physical(int value) {
    // Keep downstream clipping arithmetic bounded even for hostile/extreme
    // caller coordinates. Normal game projection stays within a few hundred.
    if (value < -32768) value = -32768;
    if (value > 32767) value = 32767;
    return value * GFX_SCALE + GFX_SCALE / 2;
}

// Fill an already rasterized horizontal span directly in the packed 4-bit
// framebuffer. Edge pixels preserve the neighboring nibble; interior pixel
// pairs can be written a byte at a time.
static void fill_span(int x0, int x1, int y, uint8_t col) {
    if ((unsigned)y >= GFX_HEIGHT) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x1 < 0 || x0 >= GFX_WIDTH) return;

    x0 = clip(x0, 0, GFX_WIDTH - 1);
    x1 = clip(x1, 0, GFX_WIDTH - 1);

    uint8_t *dst = fb4 + y * (GFX_WIDTH / 2) + (x0 >> 1);
    if (x0 & 1) {
        *dst = (uint8_t)((*dst & 0x0f) | (col << 4));
        ++dst;
        ++x0;
    }

    if (x0 <= x1) {
        const int pairs = (x1 - x0 + 1) >> 1;
        if (pairs > 0) {
            memset(dst, (uint8_t)(col | (col << 4)), (size_t)pairs);
            dst += pairs;
            x0 += pairs * 2;
        }
        if (x0 <= x1) {
            *dst = (uint8_t)((*dst & 0xf0) | col);
        }
    }
}

static void fill_logical_pixel(int x, int y, uint8_t col) {
    if (!in_logical_bounds(x, y)) return;
    const int first_y = y * GFX_SCALE;
    const int last_y = first_y + GFX_SCALE - 1;
    const int first_x = x * GFX_SCALE;
    const int last_x = first_x + GFX_SCALE - 1;
    for (int py = first_y; py <= last_y; ++py) {
        fill_span(first_x, last_x, py, col);
    }
}

static inline void swap_point(int *x0, int *y0, int *x1, int *y1) {
    int t = *x0; *x0 = *x1; *x1 = t;
    t = *y0; *y0 = *y1; *y1 = t;
}

#define TRI_FP_SHIFT 16
#define TRI_FP_ONE ((int64_t)1 << TRI_FP_SHIFT)

static inline int fixed_floor(int64_t value) {
    int64_t result = value / TRI_FP_ONE;
    if (value < 0 && value % TRI_FP_ONE) --result;
    return (int)result;
}

static inline int fixed_ceil(int64_t value) {
    int64_t result = value / TRI_FP_ONE;
    if (value > 0 && value % TRI_FP_ONE) ++result;
    return (int)result;
}

// --- Public drawing API ---

void gfx_init(void) {
    memset(fb4, 0, FB4_SIZE);
#if !GFX_SKIP_RGB565
    memset(fb_dvi, 0, sizeof(fb_dvi));
#endif
}

uint16_t *gfx_get_dvi_buffer(void) {
    return fb_dvi;
}

void gfx_cls(int color) {
    uint8_t c = color & 0xF;
    uint8_t pair = (c << 4) | c;
    memset(fb4, pair, FB4_SIZE);
}

void gfx_pset(int x, int y, int c) {
    fill_logical_pixel(x, y, (uint8_t)(c & 0x0f));
}

int gfx_pget(int x, int y) {
    if (in_logical_bounds(x, y)) {
        return pget_physical_fast(point_to_physical(x), point_to_physical(y));
    }
    return 0;
}

void gfx_rect(int x0, int y0, int x1, int y1, int c) {
    const uint8_t col = (uint8_t)(c & 0x0f);
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (x1 < 0 || y1 < 0 || x0 >= GFX_LOGICAL_WIDTH || y0 >= GFX_LOGICAL_HEIGHT) return;

    x0 = clip(x0, 0, GFX_LOGICAL_WIDTH - 1);
    x1 = clip(x1, 0, GFX_LOGICAL_WIDTH - 1);
    y0 = clip(y0, 0, GFX_LOGICAL_HEIGHT - 1);
    y1 = clip(y1, 0, GFX_LOGICAL_HEIGHT - 1);

    const int px0 = x0 * GFX_SCALE;
    const int px1 = (x1 + 1) * GFX_SCALE - 1;
    const int py0 = y0 * GFX_SCALE;
    const int py1 = (y1 + 1) * GFX_SCALE - 1;
    fill_span(px0, px1, py0, col);
    if (py1 != py0) fill_span(px0, px1, py1, col);
    for (int y = py0 + 1; y < py1; ++y) {
        pset_physical_fast(px0, y, col);
        if (px1 != px0) pset_physical_fast(px1, y, col);
    }
}

void gfx_rectfill(int x0, int y0, int x1, int y1, int c) {
    const uint8_t col = (uint8_t)(c & 0x0f);
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (x1 < 0 || y1 < 0 || x0 >= GFX_LOGICAL_WIDTH || y0 >= GFX_LOGICAL_HEIGHT) return;

    x0 = clip(x0, 0, GFX_LOGICAL_WIDTH - 1);
    x1 = clip(x1, 0, GFX_LOGICAL_WIDTH - 1);
    y0 = clip(y0, 0, GFX_LOGICAL_HEIGHT - 1);
    y1 = clip(y1, 0, GFX_LOGICAL_HEIGHT - 1);

    const int px0 = x0 * GFX_SCALE;
    const int px1 = (x1 + 1) * GFX_SCALE - 1;
    const int py0 = y0 * GFX_SCALE;
    const int py1 = (y1 + 1) * GFX_SCALE - 1;
    for (int y = py0; y <= py1; ++y) fill_span(px0, px1, y, col);
}

static int line_outcode(int x, int y) {
    int code = 0;
    if (x < 0) code |= 1;
    else if (x >= GFX_WIDTH) code |= 2;
    if (y < 0) code |= 4;
    else if (y >= GFX_HEIGHT) code |= 8;
    return code;
}

static int clip_line_physical(int *x0, int *y0, int *x1, int *y1) {
    int code0 = line_outcode(*x0, *y0);
    int code1 = line_outcode(*x1, *y1);
    for (;;) {
        if (!(code0 | code1)) return 1;
        if (code0 & code1) return 0;

        const int code = code0 ? code0 : code1;
        int64_t x;
        int64_t y;
        if (code & 8) {
            y = GFX_HEIGHT - 1;
            x = *x0 + ((int64_t)*x1 - *x0) * (y - *y0) / ((int64_t)*y1 - *y0);
        } else if (code & 4) {
            y = 0;
            x = *x0 + ((int64_t)*x1 - *x0) * (y - *y0) / ((int64_t)*y1 - *y0);
        } else if (code & 2) {
            x = GFX_WIDTH - 1;
            y = *y0 + ((int64_t)*y1 - *y0) * (x - *x0) / ((int64_t)*x1 - *x0);
        } else {
            x = 0;
            y = *y0 + ((int64_t)*y1 - *y0) * (x - *x0) / ((int64_t)*x1 - *x0);
        }

        if (code == code0) {
            *x0 = (int)x;
            *y0 = (int)y;
            code0 = line_outcode(*x0, *y0);
        } else {
            *x1 = (int)x;
            *y1 = (int)y;
            code1 = line_outcode(*x1, *y1);
        }
    }
}

static void draw_line_physical(int x0, int y0, int x1, int y1, uint8_t col) {
    if (!clip_line_physical(&x0, &y0, &x1, &y1)) return;
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = dx > 0 ? 1 : -1;
    int sy = dy > 0 ? 1 : -1;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;

    int err = dx - dy;

    for (;;) {
        pset_physical_fast(x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

void gfx_line(int x0, int y0, int x1, int y1, int c) {
    draw_line_physical(
        point_to_physical(x0), point_to_physical(y0),
        point_to_physical(x1), point_to_physical(y1),
        (uint8_t)(c & 0x0f));
}

void gfx_trifill(int x0, int y0, int x1, int y1, int x2, int y2, int c) {
    x0 = point_to_physical(x0);
    y0 = point_to_physical(y0);
    x1 = point_to_physical(x1);
    y1 = point_to_physical(y1);
    x2 = point_to_physical(x2);
    y2 = point_to_physical(y2);

    // Sorting by Y makes the two short edges meet at vertex 1 regardless of
    // the caller's winding order. Horizontal spans handle left/right order.
    if (y0 > y1) swap_point(&x0, &y0, &x1, &y1);
    if (y1 > y2) swap_point(&x1, &y1, &x2, &y2);
    if (y0 > y1) swap_point(&x0, &y0, &x1, &y1);

    int min_x = x0;
    int max_x = x0;
    if (x1 < min_x) min_x = x1;
    if (x2 < min_x) min_x = x2;
    if (x1 > max_x) max_x = x1;
    if (x2 > max_x) max_x = x2;
    if (y2 < 0 || y0 >= GFX_HEIGHT || max_x < 0 || min_x >= GFX_WIDTH) return;

    const uint8_t col = (uint8_t)(c & 0x0f);
    if (y0 == y2) {
        fill_span(min_x, max_x, y0, col);
        return;
    }

    int first_y = y0 < 0 ? 0 : y0;
    int last_y = y2 >= GFX_HEIGHT ? GFX_HEIGHT - 1 : y2;

    const int64_t long_dy = (int64_t)y2 - y0;
    const int64_t long_step = (((int64_t)x2 - x0) * TRI_FP_ONE) / long_dy;
    int64_t long_x = (int64_t)x0 * TRI_FP_ONE +
        long_step * ((int64_t)first_y - y0);

    int lower_half = first_y >= y1 && y1 < y2;
    int64_t short_step;
    int64_t short_x;
    if (lower_half) {
        const int64_t short_dy = (int64_t)y2 - y1;
        short_step = (((int64_t)x2 - x1) * TRI_FP_ONE) / short_dy;
        short_x = (int64_t)x1 * TRI_FP_ONE +
            short_step * ((int64_t)first_y - y1);
    } else {
        const int64_t short_dy = (int64_t)y1 - y0;
        short_step = (((int64_t)x1 - x0) * TRI_FP_ONE) / short_dy;
        short_x = (int64_t)x0 * TRI_FP_ONE +
            short_step * ((int64_t)first_y - y0);
    }

    for (int y = first_y; y <= last_y; ++y) {
        if (!lower_half && y == y1 && y1 < y2) {
            const int64_t short_dy = (int64_t)y2 - y1;
            lower_half = 1;
            short_step = (((int64_t)x2 - x1) * TRI_FP_ONE) / short_dy;
            short_x = (int64_t)x1 * TRI_FP_ONE;
        }

        const int64_t left = long_x < short_x ? long_x : short_x;
        const int64_t right = long_x > short_x ? long_x : short_x;
        const int span_left = fixed_ceil(left);
        const int span_right = fixed_floor(right);
        if (span_left <= span_right) fill_span(span_left, span_right, y, col);
        long_x += long_step;
        short_x += short_step;
    }
}

void gfx_circ(int cx, int cy, int r, int c) {
    const uint8_t col = (uint8_t)(c & 0x0f);
    if (r < 0) return;
    if (r == 0) {
        gfx_pset(cx, cy, col);
        return;
    }
    cx = point_to_physical(cx);
    cy = point_to_physical(cy);
    if (r > 32767) r = 32767;
    r *= GFX_SCALE;
    int x = r, y = 0, d = 1 - r;
    while (x >= y) {
        if (in_physical_bounds(cx+x, cy+y)) pset_physical_fast(cx+x, cy+y, col);
        if (in_physical_bounds(cx-x, cy+y)) pset_physical_fast(cx-x, cy+y, col);
        if (in_physical_bounds(cx+x, cy-y)) pset_physical_fast(cx+x, cy-y, col);
        if (in_physical_bounds(cx-x, cy-y)) pset_physical_fast(cx-x, cy-y, col);
        if (in_physical_bounds(cx+y, cy+x)) pset_physical_fast(cx+y, cy+x, col);
        if (in_physical_bounds(cx-y, cy+x)) pset_physical_fast(cx-y, cy+x, col);
        if (in_physical_bounds(cx+y, cy-x)) pset_physical_fast(cx+y, cy-x, col);
        if (in_physical_bounds(cx-y, cy-x)) pset_physical_fast(cx-y, cy-x, col);
        y++;
        if (d < 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

static void hline(int x0, int x1, int y, uint8_t col) {
    fill_span(x0, x1, y, col);
}

void gfx_circfill(int cx, int cy, int r, int c) {
    const uint8_t col = (uint8_t)(c & 0x0f);
    if (r < 0) return;
    if (r == 0) {
        gfx_pset(cx, cy, col);
        return;
    }
    cx = point_to_physical(cx);
    cy = point_to_physical(cy);
    if (r > 32767) r = 32767;
    r *= GFX_SCALE;
    int x = r, y = 0, d = 1 - r;
    while (x >= y) {
        hline(cx - x, cx + x, cy + y, col);
        hline(cx - x, cx + x, cy - y, col);
        hline(cx - y, cx + y, cy + x, col);
        hline(cx - y, cx + y, cy - x, col);
        y++;
        if (d < 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

static inline void ellipse_plot_physical(int x, int y, uint8_t col) {
    if (in_physical_bounds(x, y)) pset_physical_fast(x, y, col);
}

static int ellipse_physical_radius(int radius) {
    return radius * GFX_SCALE;
}

void gfx_ellipse(int cx, int cy, int rx, int ry, int c) {
    const uint8_t col = (uint8_t)(c & 0x0f);
    if (rx < 0 || ry < 0) return;
    const int max_radius = 32767 / GFX_SCALE;
    if (rx > max_radius) rx = max_radius;
    if (ry > max_radius) ry = max_radius;
    if (rx == 0 && ry == 0) {
        gfx_pset(cx, cy, col);
        return;
    }
    if (rx == 0) {
        gfx_line(cx, cy - ry, cx, cy + ry, col);
        return;
    }
    if (ry == 0) {
        gfx_line(cx - rx, cy, cx + rx, cy, col);
        return;
    }

    cx = point_to_physical(cx);
    cy = point_to_physical(cy);
    rx = ellipse_physical_radius(rx);
    ry = ellipse_physical_radius(ry);

    const int64_t rx2 = (int64_t)rx * rx;
    const int64_t ry2 = (int64_t)ry * ry;
    const int64_t limit = rx2 * ry2;

    // Sample once per row and once per column. The paired passes keep both
    // shallow and steep sections connected without floating-point work.
    int x = rx;
    for (int y = 0; y <= ry; ++y) {
        const int64_t y_term = (int64_t)y * y * rx2;
        while (x > 0 && (int64_t)x * x * ry2 + y_term > limit) --x;
        ellipse_plot_physical(cx + x, cy + y, col);
        ellipse_plot_physical(cx - x, cy + y, col);
        ellipse_plot_physical(cx + x, cy - y, col);
        ellipse_plot_physical(cx - x, cy - y, col);
    }

    int y = ry;
    for (x = 0; x <= rx; ++x) {
        const int64_t x_term = (int64_t)x * x * ry2;
        while (y > 0 && x_term + (int64_t)y * y * rx2 > limit) --y;
        ellipse_plot_physical(cx + x, cy + y, col);
        ellipse_plot_physical(cx - x, cy + y, col);
        ellipse_plot_physical(cx + x, cy - y, col);
        ellipse_plot_physical(cx - x, cy - y, col);
    }
}

void gfx_ellipsefill(int cx, int cy, int rx, int ry, int c) {
    const uint8_t col = (uint8_t)(c & 0x0f);
    if (rx < 0 || ry < 0) return;
    const int max_radius = 32767 / GFX_SCALE;
    if (rx > max_radius) rx = max_radius;
    if (ry > max_radius) ry = max_radius;
    if (rx == 0 && ry == 0) {
        gfx_pset(cx, cy, col);
        return;
    }
    if (rx == 0) {
        gfx_line(cx, cy - ry, cx, cy + ry, col);
        return;
    }
    if (ry == 0) {
        gfx_line(cx - rx, cy, cx + rx, cy, col);
        return;
    }

    cx = point_to_physical(cx);
    cy = point_to_physical(cy);
    rx = ellipse_physical_radius(rx);
    ry = ellipse_physical_radius(ry);

    const int64_t rx2 = (int64_t)rx * rx;
    const int64_t ry2 = (int64_t)ry * ry;
    const int64_t limit = rx2 * ry2;
    int x = rx;
    for (int y = 0; y <= ry; ++y) {
        const int64_t y_term = (int64_t)y * y * rx2;
        while (x > 0 && (int64_t)x * x * ry2 + y_term > limit) --x;
        fill_span(cx - x, cx + x, cy + y, col);
        if (y != 0) fill_span(cx - x, cx + x, cy - y, col);
    }
}

static void draw_ascii_char(unsigned char ch, int x, int y, uint8_t color) {
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t *glyph = font_4x6[ch - 32];
    for (int row = 0; row < 6; ++row) {
        const uint8_t bits = glyph[row];
        for (int column = 0; column < 4; ++column) {
            if (bits & (1u << column)) {
                const int px = x + column;
                const int py = y + row;
                fill_logical_pixel(px, py, color);
            }
        }
    }
}

void gfx_print_w(const char *str, int x, int y, int c, int char_w) {
    const uint8_t color = c & 0x0f;
    const int origin_x = x;

    while (*str) {
        const unsigned char ch = (unsigned char)*str++;
        if (ch == '\n') {
            x = origin_x;
            y += 6;
            continue;
        }
        draw_ascii_char(ch, x, y, color);
        x += char_w;
    }
}

void gfx_print(const char *str, int x, int y, int c) {
    gfx_print_w(str, x, y, c, 4);
}

int gfx_text_width(const char *str, int char_w) {
    int width = 0;
    while (*str && *str != '\n') {
        ++str;
        width += char_w;
    }
    return width;
}

void gfx_flip(void) {
#if GFX_SKIP_RGB565
    return;
#else
    // Convert 4-bit indexed framebuffer to RGB565 via palette LUT
    // Low nibble = left (even x), high nibble = right (odd x).
    // Apply display palette if set (screen-level color remapping for fade/flash effects)
    if (gfx_disp_pal) {
        for (int i = 0; i < GFX_WIDTH * GFX_HEIGHT / 2; i++) {
            uint8_t pair = fb4[i];
            fb_dvi[i * 2    ] = palette[gfx_disp_pal[pair & 0xF]];
            fb_dvi[i * 2 + 1] = palette[gfx_disp_pal[(pair >> 4) & 0xF]];
        }
    } else {
        for (int i = 0; i < GFX_WIDTH * GFX_HEIGHT / 2; i++) {
            uint8_t pair = fb4[i];
            fb_dvi[i * 2    ] = palette[pair & 0xF];
            fb_dvi[i * 2 + 1] = palette[(pair >> 4) & 0xF];
        }
    }
#endif
}

void gfx_set_fb(uint8_t *buf) {
    fb4 = buf ? buf : fb4_default;
}

uint8_t *gfx_get_fb(void) {
    return fb4;
}

void gfx_set_display_pal(const uint8_t *pal) {
    gfx_disp_pal = pal;
}
