#include "Screens.h"

#include "SaverScenes.h"

namespace {

const unsigned long RESUME_GAP_MS = 2000;
const unsigned long SCENE_MAX_MS = 60000;
const unsigned long MAX_FRAME_MS = 100;

int scene = NUM_SAVER_SCENES - 1;
bool started = false;
unsigned long sceneStart = 0;
unsigned long lastDraw = 0;

}  // namespace

void Screens::drawSaver(Ui::Surface& ui, const AppState& state, int) {
  const unsigned long now = millis();
  const bool hasWeather = !isnan(state.weather.temp) && state.weather.weather_code != -1;

  // Scenes that need weather sit out until it has loaded, unless nothing else is enabled.
  uint16_t mask = 0;
  for (int i = 0; i < NUM_SAVER_SCENES; i++) {
    bool usable = !SAVER_SCENES[i].needsWeather || hasWeather;
    if (usable && (state.config.saver_mask & (1 << i))) mask |= 1 << i;
  }
  if (mask == 0) mask = state.config.saver_mask;

  // A gap since the last frame means the rotation left this screen and came back.
  const bool resumed = started && now - lastDraw < RESUME_GAP_MS;
  const bool expired = now - sceneStart >= SCENE_MAX_MS;
  const bool disabled = !(mask & (1 << scene));
  if (!resumed || expired || disabled) {
    for (int i = 1; i <= NUM_SAVER_SCENES; i++) {
      int candidate = (scene + i) % NUM_SAVER_SCENES;
      if (mask & (1 << candidate)) { scene = candidate; break; }
    }
    SAVER_SCENES[scene].init(micros());
    started = true;
    sceneStart = now;
    lastDraw = now;
  }

  const unsigned long frameMs = now - lastDraw;
  SaverInput in;
  in.ms = now - sceneStart;
  in.dtMs = frameMs > MAX_FRAME_MS ? MAX_FRAME_MS : frameMs;
  in.hasWeather = hasWeather;
  in.temp = hasWeather ? (int)round(state.weather.temp) : 0;
  in.weatherCode = state.weather.weather_code;
  lastDraw = now;

  SAVER_SCENES[scene].draw(ui.raw(), in);
}
