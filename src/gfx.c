/* 128x128 indexed-color graphics for the Fruit Jam arcade foundation. */

#include "gfx.h"
#include <string.h>

// --- Framebuffers ---

// 4-bit indexed framebuffer: 2 pixels per byte, 128*128/2 = 8192 bytes
// Low nibble = left pixel (even x), high nibble = right pixel (odd x).
#define FB4_SIZE (GFX_WIDTH * GFX_HEIGHT / 2)
static uint8_t fb4_default[FB4_SIZE];
static uint8_t *fb4 = fb4_default;

// RGB565 DVI framebuffer: 128*128*2 = 32768 bytes
static uint16_t fb_dvi[GFX_WIDTH * GFX_HEIGHT];

// Display palette: if set, maps framebuffer color → display color before palette lookup
static const uint8_t *gfx_disp_pal = NULL;

// --- 16-color RGB565 palette, big-endian for DVI with BSWAP ---
// Standard RGB888 -> RGB565: R>>3 << 11 | G>>2 << 5 | B>>3

static const uint16_t palette[16] = {
    0x0000, // 0: #000000 black
    0x194A, // 1: #1D2B53 dark blue
    0x792A, // 2: #7E2553 dark purple
    0x042A, // 3: #008751 dark green
    0xAA86, // 4: #AB5236 brown
    0x5AA9, // 5: #5F574F dark grey
    0xC618, // 6: #C2C3C7 light grey
    0xFFBD, // 7: #FFF1E8 white
    0xF809, // 8: #FF004D red
    0xFD00, // 9: #FFA300 orange
    0xFF64, // 10: #FFEC27 yellow
    0x0726, // 11: #00E436 green
    0x2D7F, // 12: #29ADFF blue
    0x83B3, // 13: #83769C indigo
    0xFBB5, // 14: #FF77A8 pink
    0xFE75, // 15: #FFCCAA peach
};

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

static inline void pset_fast(int x, int y, uint8_t c) {
    int idx = y * (GFX_WIDTH / 2) + (x >> 1);
    if (x & 1)
        fb4[idx] = (fb4[idx] & 0x0F) | (c << 4);
    else
        fb4[idx] = (fb4[idx] & 0xF0) | c;
}

static inline uint8_t pget_fast(int x, int y) {
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

static inline int in_bounds(int x, int y) {
    return (unsigned)x < GFX_WIDTH && (unsigned)y < GFX_HEIGHT;
}

// --- Public drawing API ---

void gfx_init(void) {
    memset(fb4, 0, FB4_SIZE);
    memset(fb_dvi, 0, sizeof(fb_dvi));
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
    if (in_bounds(x, y))
        pset_fast(x, y, c & 0xF);
}

int gfx_pget(int x, int y) {
    if (in_bounds(x, y))
        return pget_fast(x, y);
    return 0;
}

void gfx_rect(int x0, int y0, int x1, int y1, int c) {
    uint8_t col = c & 0xF;
    // Normalize
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    x0 = clip(x0, 0, GFX_WIDTH - 1);
    x1 = clip(x1, 0, GFX_WIDTH - 1);
    y0 = clip(y0, 0, GFX_HEIGHT - 1);
    y1 = clip(y1, 0, GFX_HEIGHT - 1);

    for (int x = x0; x <= x1; x++) { pset_fast(x, y0, col); pset_fast(x, y1, col); }
    for (int y = y0 + 1; y < y1; y++) { pset_fast(x0, y, col); pset_fast(x1, y, col); }
}

void gfx_rectfill(int x0, int y0, int x1, int y1, int c) {
    uint8_t col = c & 0xF;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    x0 = clip(x0, 0, GFX_WIDTH - 1);
    x1 = clip(x1, 0, GFX_WIDTH - 1);
    y0 = clip(y0, 0, GFX_HEIGHT - 1);
    y1 = clip(y1, 0, GFX_HEIGHT - 1);

    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            pset_fast(x, y, col);
}

void gfx_line(int x0, int y0, int x1, int y1, int c) {
    uint8_t col = c & 0xF;
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = dx > 0 ? 1 : -1;
    int sy = dy > 0 ? 1 : -1;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;

    int err = dx - dy;

    for (;;) {
        if (in_bounds(x0, y0)) pset_fast(x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

void gfx_circ(int cx, int cy, int r, int c) {
    uint8_t col = c & 0xF;
    if (r < 0) return;
    int x = r, y = 0, d = 1 - r;
    while (x >= y) {
        if (in_bounds(cx+x, cy+y)) pset_fast(cx+x, cy+y, col);
        if (in_bounds(cx-x, cy+y)) pset_fast(cx-x, cy+y, col);
        if (in_bounds(cx+x, cy-y)) pset_fast(cx+x, cy-y, col);
        if (in_bounds(cx-x, cy-y)) pset_fast(cx-x, cy-y, col);
        if (in_bounds(cx+y, cy+x)) pset_fast(cx+y, cy+x, col);
        if (in_bounds(cx-y, cy+x)) pset_fast(cx-y, cy+x, col);
        if (in_bounds(cx+y, cy-x)) pset_fast(cx+y, cy-x, col);
        if (in_bounds(cx-y, cy-x)) pset_fast(cx-y, cy-x, col);
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
    if ((unsigned)y >= GFX_HEIGHT) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    x0 = clip(x0, 0, GFX_WIDTH - 1);
    x1 = clip(x1, 0, GFX_WIDTH - 1);
    for (int x = x0; x <= x1; x++)
        pset_fast(x, y, col);
}

void gfx_circfill(int cx, int cy, int r, int c) {
    uint8_t col = c & 0xF;
    if (r < 0) return;
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

static void draw_ascii_char(unsigned char ch, int x, int y, uint8_t color) {
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t *glyph = font_4x6[ch - 32];
    for (int row = 0; row < 6; ++row) {
        const uint8_t bits = glyph[row];
        for (int column = 0; column < 4; ++column) {
            if (bits & (1u << column)) {
                const int px = x + column;
                const int py = y + row;
                if (in_bounds(px, py)) pset_fast(px, py, color);
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
