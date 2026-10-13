#include "Screens.h"

#include "images.h"
#include "ScreenHost.h"

namespace {

const int SIDE_W = 40;

const unsigned char* conditionIcon(int wmoCode, bool isDay) {
  if (wmoCode == 0) return isDay ? icon_sun : icon_moon;
  if (wmoCode >= 1 && wmoCode <= 3) return isDay ? icon_cloud : icon_cloud_moon;
  if (wmoCode >= 45 && wmoCode <= 48) return icon_fog;
  if (wmoCode >= 51 && wmoCode <= 67) return icon_rain;
  if (wmoCode >= 71 && wmoCode <= 77) return icon_snow;
  if (wmoCode >= 95) return icon_thunder;
  return icon_cloud;
}

String temperature(const Config& config, float value) {
  return config.round_temps ? String((int)round(value)) : String(value, 1);
}

// The forecast: a temperature line over chance-of-rain bars, one column per hour.
const int GRAPH_X = SIDE_W + 7;
const int COLUMN_W = 6;
const int LINE_TOP = 8;
const int LINE_BOTTOM = 30;
const int BARS_BASE = 42;
const int BARS_H = 9;
const float MIN_SPAN = 4;   // degrees; keeps a steady day from drawing as a jagged one

void forecast(Ui::Canvas& c, const Config& config, const WeatherData& data) {
  float low = data.hourly_temp[0], high = low;
  for (float t : data.hourly_temp) {
    if (t < low) low = t;
    if (t > high) high = t;
  }
  c.text("H " + String((int)round(high)) + "  L " + String((int)round(low)), GRAPH_X, 0, Ui::FONT_SMALL);

  const float span = max(high - low, MIN_SPAN);
  const float floor = (high + low - span) / 2;
  {
    Ui::Group graph("forecast graph");
    int lastX = 0, lastY = 0;
    for (int i = 0; i < WEATHER_HOURS; i++) {
      const int x = GRAPH_X + i * COLUMN_W;
      const int y = LINE_BOTTOM - (int)round((data.hourly_temp[i] - floor) / span * (LINE_BOTTOM - LINE_TOP));
      if (i > 0) c.line(lastX, lastY, x + 2, y);
      lastX = x + 2;
      lastY = y;
      const int bar = (data.hourly_rain[i] * BARS_H + 50) / 100;
      if (bar > 0) c.fill(x, BARS_BASE - bar, COLUMN_W - 2, bar);
    }
    c.dotted(GRAPH_X, BARS_BASE, WEATHER_HOURS * COLUMN_W - 1);
  }

  const bool twelveHour = config.time_format == "12";
  for (int i = 0; i < WEATHER_HOURS; i += 3) {
    int hour = (data.first_hour + i) % 24;
    if (twelveHour) hour = hour % 12 == 0 ? 12 : hour % 12;
    const String label = (hour < 10 && !twelveHour ? "0" : "") + String(hour);
    c.text(label, GRAPH_X + i * COLUMN_W, BARS_BASE + 3, Ui::FONT_SMALL);
  }
}

}  // namespace

void Screens::drawWeather(Ui::Surface& ui, const AppState& state, int) {
  const Config& config = state.config;
  const WeatherData& data = state.weather;
  const bool valid = !isnan(data.temp) && data.weather_code != -1;

  const String temp = valid ? temperature(config, data.temp) : String("--");
  const String description = valid ? ScreenHost::weatherDescription(data.weather_code) : String("No Data");
  const unsigned char* picture = valid ? conditionIcon(data.weather_code, data.is_day) : icon_cloud;

  Ui::Canvas c = ui.frame("Weather");
  c.icon(picture, (SIDE_W - 24) / 2, 0, 24, 24);
  Ui::Font font = c.largestFit(temp, SIDE_W - 5, Ui::FONT_LARGE);
  const int tempX = (SIDE_W - c.textWidth(temp, font) - 5) / 2;
  c.textWithMark(temp, tempX, 26, font, degree_icon_small, 4, 4);
  c.textFitCentered(description, SIDE_W / 2, 43, SIDE_W, Ui::FONT_SMALL);
  c.vline(SIDE_W + 2, 0, c.h);

  if (valid && data.first_hour >= 0) {
    forecast(c, config, data);
  } else {
    const int graphW = c.w - GRAPH_X;
    c.textFitCentered("No Forecast", GRAPH_X + graphW / 2, (c.h - c.textHeight(Ui::FONT_SMALL)) / 2, graphW, Ui::FONT_SMALL);
  }
}
