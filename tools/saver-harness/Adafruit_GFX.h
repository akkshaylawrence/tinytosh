#ifndef HARNESS_ADAFRUIT_GFX_H
#define HARNESS_ADAFRUIT_GFX_H

// Host stand-in for the drawing calls SaverScenes uses. Unlike the real library it
// does not clip silently: every out-of-range pixel is counted.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

class Adafruit_GFX {
public:
  static const int W = 128;
  static const int H = 64;
  uint8_t pixels[W * H];
  long outOfBounds = 0;
  int minX = W, minY = H, maxX = -1, maxY = -1;

  Adafruit_GFX() { clear(); }

  void clear() { memset(pixels, 0, sizeof(pixels)); }

  void drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= W || y < 0 || y >= H) { outOfBounds++; return; }
    pixels[y * W + x] = color ? 1 : 0;
    if (color) {
      if (x < minX) minX = x;
      if (x > maxX) maxX = x;
      if (y < minY) minY = y;
      if (y > maxY) maxY = y;
    }
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++) drawPixel(x + i, y + j, color);
  }

  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    for (int i = 0; i < w; i++) { drawPixel(x + i, y, color); drawPixel(x + i, y + h - 1, color); }
    for (int j = 0; j < h; j++) { drawPixel(x, y + j, color); drawPixel(x + w - 1, y + j, color); }
  }

  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
      drawPixel(x0, y0, color);
      if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }
};

#endif
