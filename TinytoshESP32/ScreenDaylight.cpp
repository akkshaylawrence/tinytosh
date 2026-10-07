#include "Screens.h"

#include "images.h"
#include "ScreenHost.h"

void Screens::drawDaylight(Ui::Surface& ui, const AppState& state, int) {
  const Config& config = state.config;
  const DaylightData& data = state.daylight;
  if (data.sunrise_mins == -1) {
    ui.alert(icon_error, "No Daylight Data");
    return;
  }

  const struct tm now = ScreenHost::localNow();
  const int nowMins = now.tm_hour * 60 + now.tm_min;
  const bool isDay = nowMins >= data.sunrise_mins && nowMins < data.sunset_mins;

  int span, passed;
  if (isDay) {
    span = data.sunset_mins - data.sunrise_mins;
    passed = nowMins - data.sunrise_mins;
  } else {
    span = (1440 - data.sunset_mins) + data.sunrise_mins;
    passed = nowMins >= data.sunset_mins ? nowMins - data.sunset_mins : (1440 - data.sunset_mins) + nowMins;
  }
  const int percent = span > 0 ? passed * 100 / span : 0;
  const int minsLeft = span - passed;

  const int startMins = isDay ? data.sunrise_mins : data.sunset_mins;
  const int endMins = isDay ? data.sunset_mins : data.sunrise_mins;
  const int midMins = isDay ? data.noon_mins : (data.noon_mins + 720) % 1440;
  const int lengthMins = isDay ? data.length_mins : 1440 - data.length_mins;

  Ui::Canvas c = ui.frame(isDay ? "Daylight" : "Night");
  const int centres[] = {15, c.w / 2, c.w - 16};
  const unsigned char* icons[] = {isDay ? icon_sun_rise : icon_sun_set, isDay ? icon_sun : icon_moon, isDay ? icon_sun_set : icon_sun_rise};
  const int times[] = {startMins, midMins, endMins};
  const int iconY = config.daylight_minimal ? 7 : 0;

  for (int i = 0; i < 3; i++) {
    c.icon(icons[i], centres[i] - 12, iconY, 24, 24);
    c.textCentered(ScreenHost::formatMinsFromMidnight(times[i], config.time_format, false), centres[i], iconY + 27);
  }
  if (config.daylight_minimal) return;

  c.progress(0, 36, c.w, 5, percent);

  String length = ScreenHost::formatDurationMins(lengthMins);
  length.replace(" ", "");
  c.text(length, 0, 43);
  c.textCentered(String(percent) + "%", c.w / 2, 43);
  c.textRight(String(minsLeft / 60) + "h" + String(minsLeft % 60) + "m", c.w, 43);
}
