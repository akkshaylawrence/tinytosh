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

Ui::Stat statFor(const String& key, const Config& config, const WeatherData& data, bool valid) {
  Ui::Stat s;
  s.icon = icon_drop;
  s.text = "--";
  if (key == "feels") {
    s.icon = icon_feel;
    if (valid) {
      s.text = temperature(config, data.apparent_temperature);
      s.unit = degree_icon_small;
      s.unitW = 4;
      s.unitH = 4;
    }
  } else if (key == "humidity") {
    if (valid) s.text = String(data.humidity) + "%";
  } else if (key == "wind") {
    s.icon = icon_wind;
    if (valid) s.text = String((int)round(data.wind_speed)) + "km";
  } else if (key == "precipitation") {
    s.icon = icon_precipitation;
    if (valid && !isnan(data.precipitation_probability)) s.text = String((int)round(data.precipitation_probability)) + "%";
  } else if (key == "pressure") {
    s.icon = icon_pressure;
    if (valid && !isnan(data.pressure)) s.text = String((int)round(data.pressure));
  } else if (key == "visibility") {
    s.icon = icon_visibility;
    if (valid && !isnan(data.visibility)) s.text = String((int)round(data.visibility / 1000.0)) + "km";
  }
  return s;
}

}  // namespace

void Screens::drawWeather(Ui::Surface& ui, const AppState& state, int) {
  const Config& config = state.config;
  const WeatherData& data = state.weather;
  const bool valid = !isnan(data.temp) && data.weather_code != -1;

  Ui::Stat stats[6];
  int count = 0;
  for (int i = 0; i < 6; i++) {
    if (config.weather_values[i].length() > 0) stats[count++] = statFor(config.weather_values[i], config, data, valid);
  }

  const String temp = valid ? temperature(config, data.temp) : String("--");
  const String description = valid ? ScreenHost::weatherDescription(data.weather_code) : String("No Data");
  const unsigned char* picture = valid ? conditionIcon(data.weather_code, data.is_day) : icon_cloud;

  if (config.weather_show_header) {
    const String city = valid ? Ui::ascii(config.city) : String("No Location");
    Ui::Canvas c = ui.frame(city, ScreenHost::clock(config.time_format));

    Ui::Font font = c.largestFit(temp, 80, Ui::FONT_HUGE);
    c.textWithMark(temp, 0, 4 + (21 - c.textHeight(font)) / 2, font, degree_icon, 12, 12);
    c.icon(picture, c.w - 24, 0, 24, 24);
    c.textFitRight(description, c.w, 27, 70, Ui::FONT_SMALL);
    c.hline(0, 35, c.w);
    c.statRow(stats, count, 38);
    return;
  }

  Ui::Canvas c = ui.frame("Weather");
  c.icon(picture, (SIDE_W - 24) / 2, 0, 24, 24);
  Ui::Font font = c.largestFit(temp, SIDE_W - 5, Ui::FONT_LARGE);
  const int tempX = (SIDE_W - c.textWidth(temp, font) - 5) / 2;
  c.textWithMark(temp, tempX, 26, font, degree_icon_small, 4, 4);
  c.textFitCentered(description, SIDE_W / 2, 43, SIDE_W, Ui::FONT_SMALL);
  c.vline(SIDE_W + 2, 0, c.h);
  c.statGrid(stats, count, SIDE_W + 5, c.w - SIDE_W - 5);
}
