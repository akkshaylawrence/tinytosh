#include "Screens.h"

#include "SaverScenes.h"

namespace {

bool timeOn(const AppState& s) { return s.config.show_time; }
bool calendarOn(const AppState& s) { return s.config.show_calendar; }
bool weatherOn(const AppState& s) { return s.config.show_weather; }
bool airQualityOn(const AppState& s) { return s.config.show_aqi; }
bool daylightOn(const AppState& s) { return s.config.show_daylight; }
bool moonOn(const AppState& s) { return s.config.show_moon; }
bool populationOn(const AppState& s) { return s.config.show_population; }
bool currencyOn(const AppState& s) { return s.config.show_currency; }

bool flightOn(const AppState& s) {
  return s.config.show_flight && (!s.config.hide_empty_flight || Screens::flightHasData(s));
}
bool pcOn(const AppState& s) {
  return s.config.show_pc && (!s.config.hide_empty_pc || Screens::pcHasData(s));
}
bool mediaOn(const AppState& s) {
  return s.config.show_media && (!s.config.hide_empty_media || Screens::mediaHasData(s));
}
bool bambuOn(const AppState& s) {
  return s.config.show_bambu && (!s.config.hide_empty_bambu || s.bambu.status != "SYNCING");
}
bool saverOn(const AppState& s) {
  return s.config.show_saver && (s.config.saver_mask & ((1 << NUM_SAVER_SCENES) - 1)) != 0;
}

int currencyPages(const AppState& s) { return s.config.currency_count; }

bool desktopClock(const AppState& s) { return s.config.time_style == 1; }
bool always(const AppState&) { return true; }

}  // namespace

const ScreenDef SCREENS[NUM_SCREENS] = {
  {SCREEN_TIME,        timeOn,       Screens::drawTime,       nullptr,       desktopClock},
  {SCREEN_CALENDAR,    calendarOn,   Screens::drawCalendar,   nullptr,       nullptr},
  {SCREEN_WEATHER,     weatherOn,    Screens::drawWeather,    nullptr,       nullptr},
  {SCREEN_AIR_QUALITY, airQualityOn, Screens::drawAirQuality, nullptr,       nullptr},
  {SCREEN_DAYLIGHT,    daylightOn,   Screens::drawDaylight,   nullptr,       nullptr},
  {SCREEN_MOON,        moonOn,       Screens::drawMoon,       nullptr,       nullptr},
  {SCREEN_POPULATION,  populationOn, Screens::drawPopulation, nullptr,       nullptr},
  {SCREEN_FLIGHT,      flightOn,     Screens::drawFlight,     nullptr,       nullptr},
  {SCREEN_CURRENCY,    currencyOn,   Screens::drawCurrency,   currencyPages, nullptr},
  {SCREEN_PC_MONITOR,  pcOn,         Screens::drawPcMonitor,  nullptr,       nullptr},
  {SCREEN_PC_MEDIA,    mediaOn,      Screens::drawPcMedia,    nullptr,       nullptr},
  {SCREEN_BAMBU,       bambuOn,      Screens::drawBambu,      nullptr,       nullptr},
  {SCREEN_SAVER,       saverOn,      Screens::drawSaver,      nullptr,       always},
};
