// Proves the animated pages never draw outside 128x64, across worst-case inputs.
// Run: tools/saver-harness/run.sh   (frames land in tools/saver-harness/out/)

#include <stdio.h>
#include <string>

#include "SaverScenes.h"

static const int FRAME_MS = 40;
static const int SCALE = 4;
static long totalOutOfBounds = 0;

static void writeBmp(const Adafruit_GFX& g, const std::string& path) {
  const int w = Adafruit_GFX::W * SCALE, h = Adafruit_GFX::H * SCALE;
  uint8_t header[54] = {'B', 'M'};
  auto put32 = [&](int at, int v) { for (int i = 0; i < 4; i++) header[at + i] = (v >> (8 * i)) & 255; };
  put32(2, 54 + w * h * 3); put32(10, 54); put32(14, 40); put32(18, w); put32(22, h);
  header[26] = 1; header[28] = 24; put32(34, w * h * 3);
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return;
  fwrite(header, 1, 54, f);
  for (int y = h - 1; y >= 0; y--)
    for (int x = 0; x < w; x++) {
      bool lit = g.pixels[(y / SCALE) * Adafruit_GFX::W + x / SCALE];
      bool gap = (x % SCALE == SCALE - 1) || (y % SCALE == SCALE - 1);
      uint8_t v = lit ? (gap ? 120 : 235) : 8;
      uint8_t px[3] = {v, v, v};
      fwrite(px, 1, 3, f);
    }
  fclose(f);
}

static void report(const std::string& name, const Adafruit_GFX& g) {
  printf("%-36s out-of-bounds=%ld  lit x %d..%d  y %d..%d\n",
         name.c_str(), g.outOfBounds, g.minX, g.maxX, g.minY, g.maxY);
  totalOutOfBounds += g.outOfBounds;
}

static void snapshot(const Adafruit_GFX& g, const std::string& tag, int frame, int every) {
  if (every && frame % every == every / 2) writeBmp(g, "out/" + tag + "-" + std::to_string(frame / every) + ".bmp");
}

static void runScene(int index, const std::string& tag, bool hasWeather, int temp, int code, int frames, int snapshotEvery) {
  Adafruit_GFX g;
  const SaverScene& scene = SAVER_SCENES[index];
  scene.init(0xC0FFEE + index);
  for (int f = 0; f < frames; f++) {
    g.clear();
    SaverInput in{(uint32_t)f * FRAME_MS, f ? (uint32_t)FRAME_MS : 0, hasWeather, temp, code};
    scene.draw(g, in);
    snapshot(g, tag, f, snapshotEvery);
  }
  report(std::string(scene.name) + " " + tag, g);
}

static void runClock(const std::string& tag, const char* hhmm, const char* title, int snapshotEvery) {
  Adafruit_GFX g;
  const int frames = 4 * 12000 / FRAME_MS;
  for (int f = 0; f < frames; f++) {
    g.clear();
    drawDesktopClock(g, hhmm, title, (uint32_t)f * FRAME_MS);
    snapshot(g, tag, f, snapshotEvery);
  }
  report("Desktop Clock " + tag, g);
}

int main() {
  runScene(0, "paint", false, 0, 0, 3 * 16000 / FRAME_MS, 50);
  runScene(1, "spin", false, 0, 0, 6000, 35);
  runScene(2, "life", false, 0, 0, 6000, 300);

  const int codes[] = {0, 1, 2, 3, 45, 48, 51, 55, 61, 65, 67, 71, 75, 77, 80, 82, 85, 86, 95, 96, 99};
  const int temps[] = {18, 0, -5, -40, 104, -104, 9999, -999};
  for (int code : codes)
    for (int temp : temps) {
      bool keep = temp == 18 || (code == 61 && (temp == -40 || temp == 104));
      runScene(3, "weather-c" + std::to_string(code) + "-t" + std::to_string(temp), true, temp, code, 1500, keep ? 1500 : 0);
    }
  runScene(3, "weather-nodata", false, 0, 0, 1500, 1500);

  runClock("clock-24h", "23:59", "CLOCK", 25);
  runClock("clock-12h", "12:00", "Wednesday, Sep 30", 75);
  runClock("clock-longtitle", "08:08", "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWW", 0);
  runClock("clock-nodate", "--:--", "No Date", 0);

  printf("\n%s: %ld out-of-bounds pixel writes\n", totalOutOfBounds ? "FAIL" : "PASS", totalOutOfBounds);
  return totalOutOfBounds ? 1 : 0;
}
