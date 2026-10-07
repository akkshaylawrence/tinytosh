#ifndef SCREENS_H
#define SCREENS_H

// The screen registry. To add a screen:
//   1. add its id to ScreenType in structs.h, before NUM_SCREENS
//   2. write ScreenFoo.cpp with a draw function that asks the Surface for a frame
//   3. declare it below and add one row to SCREENS in Screens.cpp, in ScreenType order

#include "structs.h"
#include "Ui.h"

struct ScreenDef {
  ScreenType id;
  bool (*enabled)(const AppState& state);
  void (*draw)(Ui::Surface& ui, const AppState& state, int page);
  int (*pages)(const AppState& state);      // nullptr means one page
  bool (*animated)(const AppState& state);  // nullptr means a redraw per second is enough
};

extern const ScreenDef SCREENS[NUM_SCREENS];

namespace Screens {

void drawTime(Ui::Surface& ui, const AppState& state, int page);
void drawCalendar(Ui::Surface& ui, const AppState& state, int page);
void drawWeather(Ui::Surface& ui, const AppState& state, int page);
void drawAirQuality(Ui::Surface& ui, const AppState& state, int page);
void drawDaylight(Ui::Surface& ui, const AppState& state, int page);
void drawMoon(Ui::Surface& ui, const AppState& state, int page);
void drawPopulation(Ui::Surface& ui, const AppState& state, int page);
void drawFlight(Ui::Surface& ui, const AppState& state, int page);
void drawCurrency(Ui::Surface& ui, const AppState& state, int page);
void drawPcMonitor(Ui::Surface& ui, const AppState& state, int page);
void drawPcMedia(Ui::Surface& ui, const AppState& state, int page);
void drawBambu(Ui::Surface& ui, const AppState& state, int page);
void drawSaver(Ui::Surface& ui, const AppState& state, int page);

bool flightHasData(const AppState& state);
bool pcHasData(const AppState& state);
bool mediaHasData(const AppState& state);
bool bambuHasData(const AppState& state);

}  // namespace Screens

#endif
