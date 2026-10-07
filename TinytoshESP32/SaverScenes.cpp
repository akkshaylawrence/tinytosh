#include "SaverScenes.h"

#include <initializer_list>
#include <math.h>
#include <stdio.h>
#include <string.h>

// Everything here draws inside x 0..127, y 0..63 by construction.
// tools/saver-harness fails on any pixel requested outside that range.

namespace {

const uint16_t ON = 1;
const uint16_t OFF = 0;

const uint8_t GLYPH_DIGITS[10][5] = {
  {7,5,5,5,7}, {2,6,2,2,7}, {6,1,2,4,7}, {6,1,2,1,6}, {5,5,7,1,1},
  {7,4,6,1,6}, {3,4,7,5,7}, {7,1,2,2,2}, {7,5,7,5,7}, {7,5,7,1,6}
};
const uint8_t GLYPH_ALPHA[26][5] = {
  {2,5,7,5,5}, {6,5,6,5,6}, {3,4,4,4,3}, {6,5,5,5,6}, {7,4,6,4,7}, {7,4,6,4,4}, {3,4,5,5,3},
  {5,5,7,5,5}, {7,2,2,2,7}, {1,1,1,5,2}, {5,5,6,5,5}, {4,4,4,4,7}, {5,7,7,5,5}, {6,5,5,5,5},
  {2,5,5,5,2}, {6,5,6,4,4}, {2,5,5,6,3}, {6,5,6,5,5}, {3,4,2,1,6}, {7,2,2,2,2}, {5,5,5,5,7},
  {5,5,5,5,2}, {5,5,7,7,5}, {5,5,2,5,5}, {5,5,2,2,2}, {7,1,2,4,7}
};
const uint8_t GLYPH_COLON[5] = {0,2,0,2,0};
const uint8_t GLYPH_MINUS[5] = {0,0,7,0,0};
const uint8_t GLYPH_DOT[5]   = {0,0,0,0,2};
const uint8_t GLYPH_COMMA[5] = {0,0,0,2,4};
const uint8_t GLYPH_SLASH[5] = {1,1,2,4,4};
const uint8_t GLYPH_BLANK[5] = {0,0,0,0,0};

const uint8_t* glyph(char c) {
  if (c >= '0' && c <= '9') return GLYPH_DIGITS[c - '0'];
  if (c >= 'a' && c <= 'z') c -= 32;
  if (c >= 'A' && c <= 'Z') return GLYPH_ALPHA[c - 'A'];
  switch (c) {
    case ':': return GLYPH_COLON;
    case '-': return GLYPH_MINUS;
    case '.': return GLYPH_DOT;
    case ',': return GLYPH_COMMA;
    case '/': return GLYPH_SLASH;
    default:  return GLYPH_BLANK;
  }
}

int textWidth(const char* s, int scale) {
  int n = strlen(s);
  return n ? n * 4 * scale - scale : 0;
}

void text(Adafruit_GFX& g, const char* s, int x, int y, int scale, uint16_t color) {
  for (; *s; s++, x += 4 * scale) {
    const uint8_t* rows = glyph(*s);
    for (int j = 0; j < 5; j++)
      for (int i = 0; i < 3; i++)
        if (rows[j] & (4 >> i)) g.fillRect(x + i * scale, y + j * scale, scale, scale, color);
  }
}

void dither(Adafruit_GFX& g, int x, int y, int w, int h) {
  for (int j = y; j < y + h; j++)
    for (int i = x + ((x + j) & 1); i < x + w; i += 2) g.drawPixel(i, j, ON);
}

float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
int clampInt(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

uint32_t rngState = 0x1234567;
uint32_t rnd() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  return rngState;
}
void seedRng(uint32_t seed) { rngState = seed ? seed : 0x1234567; }

// ---- Life ----

const int LIFE_W = 64;
const int LIFE_H = 32;
const uint32_t LIFE_STEP_MS = 120;
const int LIFE_MAX_GENERATIONS = 240;

uint64_t lifeNow[LIFE_H];
uint64_t lifeNext[LIFE_H];
uint32_t lifeAccMs;
int lifeGeneration;

void lifeSeed() {
  for (int y = 0; y < LIFE_H; y++) {
    uint64_t a = ((uint64_t)rnd() << 32) | rnd();
    uint64_t b = ((uint64_t)rnd() << 32) | rnd();
    lifeNow[y] = a & b;
  }
  lifeGeneration = 0;
}

void lifeInit(uint32_t seed) {
  seedRng(seed);
  lifeAccMs = 0;
  lifeSeed();
}

bool lifeCell(int x, int y) {
  return (lifeNow[(y + LIFE_H) % LIFE_H] >> ((x + LIFE_W) % LIFE_W)) & 1;
}

void lifeStep() {
  bool changed = false;
  for (int y = 0; y < LIFE_H; y++) {
    uint64_t row = 0;
    for (int x = 0; x < LIFE_W; x++) {
      int n = lifeCell(x - 1, y - 1) + lifeCell(x, y - 1) + lifeCell(x + 1, y - 1)
            + lifeCell(x - 1, y)                          + lifeCell(x + 1, y)
            + lifeCell(x - 1, y + 1) + lifeCell(x, y + 1) + lifeCell(x + 1, y + 1);
      if (n == 3 || (n == 2 && lifeCell(x, y))) row |= (uint64_t)1 << x;
    }
    if (row != lifeNow[y]) changed = true;
    lifeNext[y] = row;
  }
  memcpy(lifeNow, lifeNext, sizeof(lifeNow));
  if (!changed || ++lifeGeneration >= LIFE_MAX_GENERATIONS) lifeSeed();
}

void lifeDraw(Adafruit_GFX& g, const SaverInput& in) {
  lifeAccMs += in.dtMs;
  if (lifeAccMs >= LIFE_STEP_MS) {
    lifeAccMs = 0;
    lifeStep();
  }
  for (int y = 0; y < LIFE_H; y++)
    for (int x = 0; x < LIFE_W; x++)
      if ((lifeNow[y] >> x) & 1) g.fillRect(x * 2, y * 2, 2, 2, ON);
}

// ---- Spinning Mac ----

// Scale and centre are sized to the worst-case projection over a full turn:
// the near bottom edge reaches 1.73 * scale below centre, the far top edge 1.56 * scale above.
const float SPIN_SCALE = 17.0f;
const int SPIN_CX = 64;
const int SPIN_CY = 31;
const float SPIN_TILT = 0.3f;
const float SPIN_PERSPECTIVE = 0.06f;

struct Vec3 { float x, y, z; };

void spinInit(uint32_t) {}

void spinDraw(Adafruit_GFX& g, const SaverInput& in) {
  const float a = in.ms * 0.0009f;
  const float ca = cosf(a), sa = sinf(a);
  const float ct = cosf(SPIN_TILT), st = sinf(SPIN_TILT);

  auto line = [&](Vec3 p, Vec3 q) {
    int sx[2], sy[2];
    const Vec3 v[2] = {p, q};
    for (int i = 0; i < 2; i++) {
      float X = v[i].x * ca + v[i].z * sa;
      float Z = -v[i].x * sa + v[i].z * ca;
      float Y = v[i].y * ct - Z * st;
      float depth = v[i].y * st + Z * ct;
      float k = SPIN_SCALE / (1 + depth * SPIN_PERSPECTIVE);
      sx[i] = SPIN_CX + (int)lroundf(X * k);
      sy[i] = SPIN_CY + (int)lroundf(Y * k);
    }
    g.drawLine(sx[0], sy[0], sx[1], sy[1], ON);
  };

  const float w = 1, h = 1.3f, d = 0.9f;
  for (float z : {-d, d}) {
    line({-w, -h, z}, {w, -h, z});
    line({w, -h, z}, {w, h, z});
    line({w, h, z}, {-w, h, z});
    line({-w, h, z}, {-w, -h, z});
  }
  for (float x : {-w, w})
    for (float y : {-h, h}) line({x, y, -d}, {x, y, d});

  if (ca <= 0) return;
  const float f = -d;
  line({-0.7f, -1, f}, {0.7f, -1, f});
  line({0.7f, -1, f}, {0.7f, 0.15f, f});
  line({0.7f, 0.15f, f}, {-0.7f, 0.15f, f});
  line({-0.7f, 0.15f, f}, {-0.7f, -1, f});
  line({0.1f, 0.7f, f}, {0.7f, 0.7f, f});
  line({-0.3f, -0.65f, f}, {-0.3f, -0.35f, f});
  line({0.3f, -0.65f, f}, {0.3f, -0.35f, f});
  line({-0.3f, -0.1f, f}, {0.3f, -0.1f, f});
}

// ---- Weather ----

enum Sky { SKY_NONE, SKY_CLEAR, SKY_CLOUD, SKY_FOG, SKY_RAIN, SKY_SNOW, SKY_STORM };

Sky skyFor(bool hasWeather, int code) {
  if (!hasWeather) return SKY_NONE;
  if (code == 0) return SKY_CLEAR;
  if (code >= 45 && code <= 48) return SKY_FOG;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return SKY_SNOW;
  if (code >= 95) return SKY_STORM;
  if (code >= 51 && code <= 82) return SKY_RAIN;
  return SKY_CLOUD;
}

const char* skyLabel(Sky sky) {
  switch (sky) {
    case SKY_CLEAR: return "CLEAR";
    case SKY_CLOUD: return "CLOUDY";
    case SKY_FOG:   return "FOG";
    case SKY_RAIN:  return "RAIN";
    case SKY_SNOW:  return "SNOW";
    case SKY_STORM: return "STORM";
    default:        return "NO DATA";
  }
}

const int NUM_PARTICLES = 40;
const int WEATHER_MAX_TEXT_W = 96;
struct Particle { float x, y, v; };
Particle particles[NUM_PARTICLES];

void weatherInit(uint32_t seed) {
  seedRng(seed);
  for (Particle& p : particles) {
    p.x = rnd() % 128;
    p.y = rnd() % 64;
    p.v = 50 + rnd() % 30;
  }
}

void weatherParticles(Adafruit_GFX& g, Sky sky, const SaverInput& in) {
  const float dt = in.dtMs / 1000.0f;
  for (int i = 0; i < NUM_PARTICLES; i++) {
    Particle& p = particles[i];
    switch (sky) {
      case SKY_RAIN:
      case SKY_STORM: {
        float v = p.v * (sky == SKY_STORM ? 1.5f : 1.0f);
        p.y += v * dt;
        p.x -= v * dt * 0.3f;
        if (p.y > 60 || p.x < 1) { p.y = 0; p.x = 1 + rnd() % 145; }
        int x = (int)p.x, y = (int)p.y;
        if (x >= 1 && x <= 127 && y >= 0 && y <= 60) g.drawLine(x, y, x - 1, y + 3, ON);
        break;
      }
      case SKY_SNOW: {
        p.y += p.v * 0.22f * dt;
        p.x += sinf(in.ms / 700.0f + i) * 6 * dt;
        if (p.y > 63) { p.y = 0; p.x = rnd() % 128; }
        int x = (int)p.x, y = (int)p.y;
        if (x >= 0 && x <= 127 && y >= 0 && y <= 63) g.drawPixel(x, y, ON);
        break;
      }
      case SKY_CLEAR: {
        bool dimmed = ((in.ms / 450 + i * 7) % 9) == 0;
        if (!dimmed) g.drawPixel((int)p.x % 128, (int)p.y % 64, ON);
        break;
      }
      case SKY_CLOUD:
      case SKY_FOG: {
        int len = (sky == SKY_FOG ? 14 : 6) + (i % 5) * 2;
        p.x -= p.v * 0.15f * dt;
        if (p.x < -len) p.x = 128;
        int x0 = clampInt((int)p.x, 0, 127), x1 = clampInt((int)p.x + len, 0, 127);
        int y = clampInt((int)p.y, 0, 63);
        if (x1 > x0) g.drawLine(x0, y, x1, y, ON);
        break;
      }
      default: break;
    }
  }
}

void weatherDraw(Adafruit_GFX& g, const SaverInput& in) {
  const Sky sky = skyFor(in.hasWeather, in.weatherCode);
  weatherParticles(g, sky, in);

  char temp[8];
  if (in.hasWeather) snprintf(temp, sizeof(temp), "%d", clampInt(in.temp, -999, 9999));
  else strcpy(temp, "--");
  const char* label = skyLabel(sky);

  const int degreeW = in.hasWeather ? 7 : 0;
  int scale = 4;
  if (textWidth(temp, scale) + degreeW > WEATHER_MAX_TEXT_W) scale = 3;
  const int tempW = textWidth(temp, scale) + degreeW;
  const int labelW = textWidth(label, 2);
  const int boxW = (tempW > labelW ? tempW : labelW) + 10;

  const int tempY = 13;
  const int labelY = tempY + 5 * scale + 7;
  g.fillRect((128 - boxW) / 2, tempY - 4, boxW, labelY + 10 + 4 - (tempY - 4), OFF);

  const int tempX = (128 - tempW) / 2;
  text(g, temp, tempX, tempY, scale, ON);
  if (in.hasWeather) {
    const int dx = tempX + tempW - 5;
    g.drawRect(dx, tempY, 5, 5, ON);
    g.drawPixel(dx, tempY, OFF);
    g.drawPixel(dx + 4, tempY, OFF);
    g.drawPixel(dx, tempY + 4, OFF);
    g.drawPixel(dx + 4, tempY + 4, OFF);
  }
  text(g, label, (128 - labelW) / 2, labelY, 2, ON);
}

// ---- MacPaint ----

const uint32_t PAINT_LOOP_MS = 16000;
const int PAINT_LEFT = 16;
const int PAINT_RIGHT = 125;
const int PAINT_BOTTOM = 61;
const int SUN_X = 100, SUN_Y = 18, SUN_R = 9;
const int MOUNTAIN[][2] = {{16, 52}, {40, 26}, {58, 44}, {78, 30}, {104, 50}, {125, 40}};
const int MOUNTAIN_POINTS = sizeof(MOUNTAIN) / sizeof(MOUNTAIN[0]);

int mountainY(int x) {
  for (int i = 0; i < MOUNTAIN_POINTS - 1; i++) {
    const int* a = MOUNTAIN[i];
    const int* b = MOUNTAIN[i + 1];
    if (x >= a[0] && x <= b[0]) return a[1] + (b[1] - a[1]) * (x - a[0]) / (b[0] - a[0]);
  }
  return PAINT_BOTTOM;
}

void paintInit(uint32_t) {}

void paintDraw(Adafruit_GFX& g, const SaverInput& in) {
  const float c = (in.ms % PAINT_LOOP_MS) / (float)PAINT_LOOP_MS;
  const int phase = c < 0.25f ? 0 : c < 0.5f ? 1 : c < 0.8f ? 2 : c < 0.9f ? 3 : 4;
  const int span = PAINT_RIGHT - PAINT_LEFT;

  g.drawRect(14, 0, 114, 64, ON);
  int cx = SUN_X + SUN_R, cy = SUN_Y;

  const float sunTurn = clamp01(c / 0.22f) * 6.2832f;
  for (float a = 0; a < sunTurn; a += 0.06f) {
    cx = SUN_X + (int)lroundf(SUN_R * cosf(a));
    cy = SUN_Y + (int)lroundf(SUN_R * sinf(a));
    g.drawPixel(cx, cy, ON);
  }
  if (phase >= 1) {
    for (int k = 0; k < 8; k++) {
      float a = k * 0.7854f;
      g.drawLine(SUN_X + (int)lroundf(12 * cosf(a)), SUN_Y + (int)lroundf(12 * sinf(a)),
                 SUN_X + (int)lroundf(14 * cosf(a)), SUN_Y + (int)lroundf(14 * sinf(a)), ON);
    }
    int xEnd = PAINT_LEFT + (int)(clamp01((c - 0.25f) / 0.22f) * span);
    for (int x = PAINT_LEFT; x <= xEnd; x++) {
      int y = mountainY(x);
      g.drawPixel(x, y, ON);
      g.drawPixel(x, y + 1, ON);
    }
    cx = xEnd;
    cy = mountainY(xEnd);
  }
  if (phase >= 2) {
    int xEnd = PAINT_LEFT + (int)(clamp01((c - 0.5f) / 0.28f) * span);
    for (int x = PAINT_LEFT; x <= xEnd; x++)
      for (int y = mountainY(x) + 3; y <= PAINT_BOTTOM; y++)
        if ((x + y * 2) % 4 == 0) g.drawPixel(x, y, ON);
    cx = xEnd;
    cy = 57;
  }
  if (phase == 4) {
    int wipe = (int)(clamp01((c - 0.9f) / 0.09f) * 112);
    g.fillRect(PAINT_LEFT - 1, 1, wipe, 62, OFF);
    cx = PAINT_LEFT - 1 + wipe;
    cy = 32;
  }

  g.drawRect(0, 0, 14, 64, ON);
  const int tool = phase == 0 ? 2 : phase == 1 ? 7 : phase == 4 ? 13 : 10;
  for (int i = 0; i < 14; i++) {
    int x = 1 + (i % 2) * 6, y = 4 + (i / 2) * 8;
    if (i == tool) g.fillRect(x, y, 6, 8, ON);
    else g.drawRect(x, y, 6, 8, ON);
  }

  if (phase != 3) {
    cx = clampInt(cx, PAINT_LEFT + 3, PAINT_RIGHT - 3);
    cy = clampInt(cy, 5, 58);
    g.drawLine(cx - 3, cy, cx + 3, cy, ON);
    g.drawLine(cx, cy - 3, cx, cy + 3, ON);
    g.drawPixel(cx, cy, OFF);
  }
}

// ---- Desktop clock ----

const uint32_t DESK_LOOP_MS = 12000;
const int WINDOW_X = 21, WINDOW_Y = 16, WINDOW_W = 86, WINDOW_H = 34;
const int WINDOW_DRAG = 8;
const int TITLE_MAX_CHARS = 19;

const char* const ARROW[] = {
  "#o.....", "##o....", "###o...", "####o..", "#####o.",
  "######o", "###oooo", "#o##o..", "o.o##o.", "...oo.."
};
const int ARROW_W = 7, ARROW_H = 10;

void drawArrow(Adafruit_GFX& g, int x, int y) {
  x = clampInt(x, 0, 127 - ARROW_W);
  y = clampInt(y, 0, 63 - ARROW_H);
  for (int j = 0; j < ARROW_H; j++)
    for (int i = 0; i < ARROW_W; i++)
      if (ARROW[j][i] != '.') g.drawPixel(x + i, y + j, ARROW[j][i] == '#' ? ON : OFF);
}

}  // namespace

const SaverScene SAVER_SCENES[NUM_SAVER_SCENES] = {
  {"MacPaint", paintInit, paintDraw, false},
  {"Spinning Mac", spinInit, spinDraw, false},
  {"Life", lifeInit, lifeDraw, false},
  {"Weather", weatherInit, weatherDraw, true},
};

void drawDesktopClock(Adafruit_GFX& g, const char* hhmm, const char* title, uint32_t ms) {
  const float c = (ms % DESK_LOOP_MS) / 1000.0f;
  const bool dragging = c >= 3 && c < 5;
  const bool returning = c >= 11;
  const bool menuOpen = c >= 7.5f && c < 11;

  int drag = 0;
  if (dragging) drag = (int)((c - 3) / 2 * WINDOW_DRAG);
  else if (c >= 5 && c < 11) drag = WINDOW_DRAG;
  else if (returning) drag = (int)((12 - c) * WINDOW_DRAG);
  const int wx = WINDOW_X + drag;

  dither(g, 0, 10, 128, 54);
  g.fillRect(0, 0, 128, 9, ON);
  g.fillRect(3, 3, 4, 4, OFF);
  g.drawPixel(5, 2, OFF);
  text(g, "FILE EDIT VIEW", 12, 2, 1, OFF);

  g.fillRect(wx + 2, WINDOW_Y + 2, WINDOW_W, WINDOW_H, OFF);
  g.fillRect(wx, WINDOW_Y, WINDOW_W, WINDOW_H, OFF);
  g.drawRect(wx, WINDOW_Y, WINDOW_W, WINDOW_H, ON);
  for (int y = WINDOW_Y + 2; y <= WINDOW_Y + 6; y += 2) g.drawLine(wx + 2, y, wx + WINDOW_W - 3, y, ON);
  g.drawLine(wx, WINDOW_Y + 8, wx + WINDOW_W - 1, WINDOW_Y + 8, ON);

  char shortTitle[TITLE_MAX_CHARS + 1];
  strncpy(shortTitle, title, TITLE_MAX_CHARS);
  shortTitle[TITLE_MAX_CHARS] = 0;
  const int titleW = textWidth(shortTitle, 1);
  const int titleX = wx + (WINDOW_W - titleW) / 2;
  g.fillRect(titleX - 2, WINDOW_Y + 1, titleW + 4, 7, OFF);
  text(g, shortTitle, titleX, WINDOW_Y + 2, 1, ON);

  char shown[6];
  strncpy(shown, hhmm, 5);
  shown[5] = 0;
  if ((ms / 500) % 2 && strlen(shown) == 5) shown[2] = ' ';
  text(g, shown, wx + (WINDOW_W - textWidth("00:00", 3)) / 2, WINDOW_Y + 14, 3, ON);

  g.fillRect(111, 53, 9, 10, OFF);
  g.drawRect(111, 53, 9, 10, ON);
  g.drawLine(109, 52, 121, 52, ON);
  g.drawLine(114, 56, 114, 60, ON);
  g.drawLine(116, 56, 116, 60, ON);

  int cx = 64 + (int)(50 * sinf(ms * 0.0005f));
  int cy = 36 + (int)(17 * sinf(ms * 0.0008f + 1));
  if (dragging || returning) {
    cx = wx + 8;
    cy = WINDOW_Y + 3;
  }
  if (menuOpen) {
    const char* const items[] = {"NEW", "OPEN", "QUIT"};
    const bool picking = c >= 8.2f;
    int picked = (int)((c - 8.2f) / 0.8f);
    if (picked > 2) picked = 2;
    g.fillRect(10, 0, 19, 9, OFF);
    text(g, "FILE", 12, 2, 1, ON);
    g.fillRect(10, 9, 36, 25, OFF);
    g.drawRect(10, 9, 36, 25, ON);
    for (int i = 0; i < 3; i++) {
      bool highlighted = picking && i == picked;
      if (highlighted) g.fillRect(11, 11 + i * 7, 34, 7, ON);
      text(g, items[i], 14, 12 + i * 7, 1, highlighted ? OFF : ON);
    }
    cx = 26;
    cy = picking ? 13 + picked * 7 : 4;
  }
  drawArrow(g, cx, cy);
}
