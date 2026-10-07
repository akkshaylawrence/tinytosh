#include "Screens.h"

#include "images.h"
#include "ScreenHost.h"

namespace {

String withThousands(long long value) {
  String s = String(value);
  for (int p = s.length() - 3; p > 0; p -= 3) s = s.substring(0, p) + "," + s.substring(p);
  return s;
}

String growthText(double growth) {
  return String(growth > 0 ? "+" : "") + String(growth, 2) + "%";
}

struct Place {
  const unsigned char* icon;
  int iconW;
  String name;
  long long people;
  double growth;
};

void drawRow(Ui::Canvas& c, const Place& place, int y) {
  c.icon(place.icon, 0, y, place.iconW, 16);
  c.text(withThousands(place.people), 20, y);
  const String growth = growthText(place.growth);
  c.textRight(growth, c.w, y + 11, Ui::FONT_SMALL);
  c.textFit(place.name, 20, y + 11, c.w - 24 - c.textWidth(growth, Ui::FONT_SMALL), Ui::FONT_SMALL);
}

void drawSingle(Ui::Canvas& c, const Place& place) {
  const int cx = c.w / 2;
  c.icon(place.icon, cx - place.iconW / 2, 0, place.iconW, 16);
  c.textFitCentered(place.name, cx, 19, c.w);

  const String people = withThousands(place.people);
  const int peopleX = (c.w - 12 - c.textWidth(people)) / 2;
  {
    Ui::Group count(people.c_str());
    c.icon(icon_people, peopleX, 30, 8, 8);
    c.text(people, peopleX + 12, 31);
  }

  const String growth = growthText(place.growth);
  const int growthX = (c.w - 12 - c.textWidth(growth)) / 2;
  Ui::Group change(growth.c_str());
  c.icon(place.growth >= 0 ? icon_up_arrow : icon_down_arrow, growthX, 41, 8, 8);
  c.text(growth, growthX + 12, 42);
}

}  // namespace

void Screens::drawPopulation(Ui::Surface& ui, const AppState& state, int) {
  const Config& config = state.config;
  const PopulationData& data = state.population;
  if (data.world_pop_base == -1 && data.country_pop_base == -1) {
    ui.alert(icon_error, "No Populace Data");
    return;
  }

  const bool showWorld = config.pop_show_world && data.world_year > 0;
  const bool showCountry = config.pop_show_country && data.country_year > 0;
  const Place world = {icon_world, 16, "World", ScreenHost::livePopulation(data.world_pop_base, data.world_growth, data.world_year), data.world_growth};
  const Place country = {icon_location, 13, Ui::ascii(config.country), ScreenHost::livePopulation(data.country_pop_base, data.country_growth, data.country_year), data.country_growth};

  Ui::Canvas c = ui.frame("Population");
  if (showWorld && showCountry) {
    drawRow(c, world, 2);
    c.dotted(0, 23, c.w);
    drawRow(c, country, 29);
  } else if (showWorld || showCountry) {
    drawSingle(c, showWorld ? world : country);
  }
}
