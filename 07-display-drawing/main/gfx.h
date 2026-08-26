#ifndef GFX_H
#define GFX_H

#include <stdint.h>

#define GFX_WIDTH  320
#define GFX_HEIGHT 240

#define GFX_RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3)))

void gfx_fill_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color);
void gfx_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color);
void gfx_line(uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t color);
void gfx_string(uint16_t *fb, int x, int y, const char *str, uint16_t color, uint16_t bg);

#endif
