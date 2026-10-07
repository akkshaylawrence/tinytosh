#include "Screens.h"

#include "images.h"
#include "ScreenHost.h"

namespace {

const int SIDE_W = 40;

struct Pollutant {
  const char* key;
  const unsigned char* icon;
  float AirQualityData::*value;
  bool microgram;
};

const Pollutant POLLUTANTS[] = {
  {"pm25", icon_small_particles, &AirQualityData::pm25, true},
  {"pm10", icon_big_particles,   &AirQualityData::pm10, true},
  {"no2",  icon_no2,             &AirQualityData::no2,  true},
  {"co",   icon_co,              &AirQualityData::co,   true},
  {"co2",  icon_co2,             &AirQualityData::co2,  false},
  {"so2",  icon_so2,             &AirQualityData::so2,  true},
  {"o3",   icon_o3,              &AirQualityData::o3,   true},
  {"dust", icon_dust,            &AirQualityData::dust, true},
  {"uv",   icon_uv,              &AirQualityData::uv,   false},
  {"ch4",  icon_ch4,             &AirQualityData::ch4,  false},
};

const unsigned char* faceIcon(int aqi, bool european) {
  const int good = european ? 20 : 50, fair = european ? 60 : 100, poor = european ? 80 : 150;
  if (aqi <= good) return icon_smile;
  if (aqi <= fair) return icon_neutral;
  if (aqi <= poor) return icon_bad;
  return icon_dead;
}

Ui::Stat statFor(const String& key, const AirQualityData& data, bool valid) {
  const Pollutant* p = &POLLUTANTS[0];
  for (const Pollutant& candidate : POLLUTANTS) {
    if (key == candidate.key) p = &candidate;
  }
  const float value = data.*(p->value);

  Ui::Stat s;
  s.icon = p->icon;
  s.text = (!valid || isnan(value)) ? String("--") : String((int)round(value));
  if (p->microgram) {
    s.unit = icon_ug;
    s.unitW = 8;
    s.unitH = 8;
  }
  return s;
}

}  // namespace

void Screens::drawAirQuality(Ui::Surface& ui, const AppState& state, int) {
  const Config& config = state.config;
  const AirQualityData& data = state.aqi;
  const bool valid = data.aqi != -1;

  Ui::Stat stats[6];
  int count = 0;
  for (int i = 0; i < 6; i++) {
    if (config.aqi_values[i].length() > 0) stats[count++] = statFor(config.aqi_values[i], data, valid);
  }

  const String aqi = valid ? String(data.aqi) : String("--");
  const String status = valid ? data.status : String("No Data");
  const unsigned char* face = valid ? faceIcon(data.aqi, config.aqi_type == "EU") : icon_neutral;

  if (config.aqi_show_header) {
    const String city = valid ? Ui::ascii(config.city) : String("No Location");
    Ui::Canvas c = ui.frame(city, ScreenHost::clock(config.time_format));

    c.text(aqi, 0, 4, Ui::FONT_HUGE);
    const int labelX = c.textWidth(aqi, Ui::FONT_HUGE) + 4;
    c.text(config.aqi_type, labelX, 4);
    c.text("AQI", labelX, 14);
    c.icon(face, c.w - 24, 0, 24, 24);
    c.textFitRight(status, c.w, 27, 70, Ui::FONT_SMALL);
    c.hline(0, 35, c.w);
    c.statRow(stats, count, 38);
    return;
  }

  Ui::Canvas c = ui.frame(config.aqi_type + " Air Quality");
  c.icon(face, (SIDE_W - 24) / 2, 0, 24, 24);
  Ui::Font font = c.largestFit(aqi, SIDE_W, Ui::FONT_LARGE);
  c.textCentered(aqi, SIDE_W / 2, 26, font);
  c.textFitCentered(status, SIDE_W / 2, 43, SIDE_W, Ui::FONT_SMALL);
  c.vline(SIDE_W + 2, 0, c.h);
  c.statGrid(stats, count, SIDE_W + 5, c.w - SIDE_W - 5);
}
