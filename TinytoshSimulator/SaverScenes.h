#ifndef SAVER_SCENES_H
#define SAVER_SCENES_H

#include <stdint.h>
#include <Adafruit_GFX.h>

struct SaverInput {
  uint32_t ms;      // time since the scene started
  uint32_t dtMs;    // time since the previous frame
  bool hasWeather;
  int temp;
  int weatherCode;  // WMO code
};

struct SaverScene {
  const char* name;
  void (*init)(uint32_t seed);
  void (*draw)(Adafruit_GFX& g, const SaverInput& in);
  bool needsWeather;
};

enum { NUM_SAVER_SCENES = 4 };
extern const SaverScene SAVER_SCENES[NUM_SAVER_SCENES];

void drawDesktopClock(Adafruit_GFX& g, const char* hhmm, const char* title, uint32_t ms);

#endif
