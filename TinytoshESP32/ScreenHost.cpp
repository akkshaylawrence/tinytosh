#include "ScreenHost.h"

#include "PopulationService.h"
#include "TimeService.h"
#include "WeatherService.h"

namespace ScreenHost {

struct tm localNow() {
  time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  return local;
}

String clock(const String& timeFormat) { return TimeService::getCurrentTimeShort(timeFormat); }
String fullDate() { return TimeService::getFullDate(); }
String formatMinsFromMidnight(int mins, const String& timeFormat, bool showAmPm) { return TimeService::formatMinsFromMidnight(mins, timeFormat, showAmPm); }
String formatDurationMins(int mins) { return TimeService::formatDurationMins(mins); }
String weatherDescription(int wmoCode) { return WeatherService::getWeatherDescription(wmoCode); }
long long livePopulation(long long base, double growth, int year) { return PopulationService::getLivePopulation(base, growth, year); }

}  // namespace ScreenHost
