#ifndef SCREEN_HOST_H
#define SCREEN_HOST_H

// What the screens need from the device around them. The firmware answers from its services,
// the simulator and the host harness answer with fixed data. Screens include nothing else from
// the firmware, which is what lets the same screen files build in all three.

#include <Arduino.h>
#include <time.h>

namespace ScreenHost {

struct tm localNow();
String clock(const String& timeFormat);
String fullDate();
String formatMinsFromMidnight(int mins, const String& timeFormat, bool showAmPm);
String formatDurationMins(int mins);
String weatherDescription(int wmoCode);
long long livePopulation(long long base, double growth, int year);

}  // namespace ScreenHost

#endif
