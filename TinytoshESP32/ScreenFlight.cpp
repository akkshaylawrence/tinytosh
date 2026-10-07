#include "Screens.h"

#include "images.h"

namespace {

const float KM_PER_NM = 1.852f;
const float FT_TO_M = 0.3048f;

const int RADAR_SLOTS = MAX_RADAR_AIRCRAFT + 1;  // slot 0 is the fixed centre mark
const int LABEL_H = 5;
const int BADGE_GAP = 2;  // clear pixels kept between neighbouring badges

bool metric(const Config& config) { return config.flight_units == "metric"; }

String altitudeText(const Config& config, float altitudeFt, const char* gap) {
  if (isnan(altitudeFt)) return "--";
  if (metric(config)) return String((int)round(altitudeFt * FT_TO_M)) + gap + "m";
  return String((int)round(altitudeFt)) + gap + "ft";
}

String velocityText(const Config& config, float velocityKt, bool spaced) {
  if (isnan(velocityKt)) return "--";
  if (metric(config)) return String((int)round(velocityKt * KM_PER_NM)) + (spaced ? " km/h" : "kmh");
  return String((int)round(velocityKt)) + (spaced ? " kt" : "kt");
}

String distanceText(const Config& config, float distanceKm, int decimals, const char* gap) {
  const float value = metric(config) ? distanceKm : distanceKm / KM_PER_NM;
  return (value < 10 ? String(value, decimals) : String((int)round(value))) + gap + (metric(config) ? "km" : "nm");
}

String upper(String s) {
  s.toUpperCase();
  return s;
}

struct Heading {
  const unsigned char* bitmap;
  int w;
  int h;
};

Heading headingFor(float trackDeg) {
  float deg = fmod(trackDeg, 360.0f);
  if (deg < 0) deg += 360.0f;
  if (deg >= 337.5 || deg < 22.5) return {icon_flight_n, 5, 7};
  if (deg < 67.5) return {icon_flight_ne, 5, 5};
  if (deg < 112.5) return {icon_flight_e, 7, 5};
  if (deg < 157.5) return {icon_flight_se, 5, 5};
  if (deg < 202.5) return {icon_flight_s, 5, 7};
  if (deg < 247.5) return {icon_flight_sw, 5, 5};
  if (deg < 292.5) return {icon_flight_w, 7, 5};
  return {icon_flight_nw, 5, 5};
}

void drawField(Ui::Canvas& c, const unsigned char* icon, int iconW, int iconH, const String& value, int x, int y, bool degrees = false) {
  Ui::Group field(value.c_str());
  c.icon(icon, x, y + 1, iconW, iconH);
  if (degrees) c.textWithMark(value, x + 9, y, Ui::FONT_NORMAL, degree_icon_small, 4, 4);
  else c.text(value, x + 9, y);
}

void drawClosest(Ui::Surface& ui, const Config& config, const FlightData& data) {
  const FlightAircraft& ac = data.closest;
  Ui::Canvas c = ui.frame(ac.callsign);
  const int half = c.w / 2;

  c.text(ac.has_route ? ac.origin_code : String("N/A"), 0, 0, Ui::FONT_LARGE);
  c.textRight(ac.has_route ? ac.destination_code : String("N/A"), c.w, 0, Ui::FONT_LARGE);
  c.icon(icon_plane, half - 6, 1, 12, 12);

  if (ac.has_route) {
    c.textFit(Ui::ascii(data.origin_city), 0, 16, half - 4);
    c.textFitRight(Ui::ascii(data.destination_city), c.w, 16, half - 4);
  }

  const String track = isnan(ac.track_deg) ? String("--") : String((int)round(ac.track_deg));
  drawField(c, icon_flight_e, 7, 5, velocityText(config, ac.velocity_kt, true), 0, 26);
  drawField(c, icon_flight_n, 5, 7, altitudeText(config, ac.altitude_ft, " "), half + 1, 26);
  drawField(c, icon_flight_ne, 5, 5, distanceText(config, ac.distance_km, 2, " "), 0, 35);
  drawField(c, icon_flight_sw, 5, 5, track, half + 1, 35, true);

  const String details[] = {
    (ac.has_route && ac.origin_country.length() > 0 && ac.destination_country.length() > 0) ? ac.origin_country + "-" + ac.destination_country : String(""),
    Ui::ascii(data.aircraft_manufacturer), Ui::ascii(data.aircraft_type), ac.type_designator, ac.registration, ac.icao24, ac.squawk,
  };
  String line;
  for (const String& detail : details) {
    if (detail.length() == 0) continue;
    if (line.length() > 0) line += "  ";
    line += detail;
  }
  c.textFit(line, 0, 44, c.w, Ui::FONT_SMALL);
}

String badgeText(const String& info, const Config& config, const FlightAircraft& ac) {
  if (info == "callsign") return ac.callsign;
  if (info == "altitude") return altitudeText(config, ac.altitude_ft, "");
  if (info == "velocity") return velocityText(config, ac.velocity_kt, false);
  if (info == "distance") return distanceText(config, ac.distance_km, 1, "");
  if (info == "route") return ac.has_route ? ac.origin_code + "-" + ac.destination_code : String("--");
  if (info == "track") return isnan(ac.track_deg) ? String("--") : String((int)round(ac.track_deg));
  if (info == "type") return ac.type_designator.length() > 0 ? ac.type_designator : String("--");
  return "";
}

struct Badge {
  int x;
  int y;
  int boxW;
  int top;     // offsets from y
  int bottom;
  bool dropped;
  String primary;
  String secondary;
};

bool overlapsVertically(const Badge& a, const Badge& b) {
  return min(a.y + a.bottom, b.y + b.bottom) - max(a.y + a.top, b.y + b.top) > 0;
}

// Nudges colliding badges apart, then drops whatever still collides or has left the box.
void resolveOverlaps(Badge* badges, int count, int minX, int maxX, int minY, int maxY) {
  for (int pass = 0; pass < 4; pass++) {
    for (int i = 1; i < count; i++) {
      for (int j = 0; j < i; j++) {
        Badge& a = badges[i];
        const Badge& b = badges[j];
        float dx = a.x - b.x;
        const float centreA = a.y + (a.top + a.bottom) / 2.0f;
        const float centreB = b.y + (b.top + b.bottom) / 2.0f;
        if (fabs(dx) < 0.01 && fabs(centreA - centreB) < 0.01) dx = (i % 2 == 0) ? 1.0f : -1.0f;

        const float overlapY = min(a.y + a.bottom, b.y + b.bottom) - max(a.y + a.top, b.y + b.top);
        const float overlapX = (a.boxW + b.boxW) / 2.0f - fabs(dx);
        if (overlapX <= 0 || overlapY <= 0) continue;

        if (overlapX < overlapY) a.x += (int)round((dx >= 0 ? 1.0f : -1.0f) * (overlapX + 1));
        else a.y += (int)round((centreA >= centreB ? 1.0f : -1.0f) * (overlapY + 1));
      }
    }
  }

  for (int i = 1; i < count; i++) {
    Badge& a = badges[i];
    // A badge nudged past the edge is slid back in, and only dropped if that lands it on another.
    const int half = (a.boxW - BADGE_GAP + 1) / 2;
    a.x = constrain(a.x, minX + half, maxX - half);
    a.y = constrain(a.y, minY - a.top, maxY - (a.bottom - BADGE_GAP));
    for (int j = 0; j < i; j++) {
      const Badge& b = badges[j];
      if (b.dropped) continue;
      if (fabs(a.x - b.x) < (a.boxW + b.boxW) / 2.0f && overlapsVertically(a, b)) {
        a.dropped = true;
        break;
      }
    }
  }
}

void drawRadar(Ui::Surface& ui, const Config& config, const FlightData& data) {
  const String radius = metric(config)
    ? String((int)floor(config.flight_radius_nm * KM_PER_NM)) + "km"
    : String(config.flight_radius_nm) + "nm";
  Ui::Canvas c = ui.frame("Radar " + radius);

  const int centreX = c.w / 2, centreY = c.h / 2;
  const int minX = 0, maxX = c.w - 1, minY = 0, maxY = c.h - 1;
  {
    Ui::Group here("centre mark");
    c.fill(centreX - 2, centreY, 5, 1);
    c.fill(centreX, centreY - 2, 1, 5);
  }

  Badge badges[RADAR_SLOTS];
  badges[0] = {centreX, centreY, 5 + BADGE_GAP, -3, 3 + BADGE_GAP, false, "", ""};
  int count = 1;

  float cosLat = cos(radians(config.latitude));
  if (fabs(cosLat) < 0.01) cosLat = 0.01;
  const float shortAxisKm = (config.flight_radius_nm * KM_PER_NM) / sqrt(5.0f);
  const float pxPerKm = (min(centreY - minY, maxY - centreY) - 2) / shortAxisKm;
  const bool wantsSecondary = config.flight_secondary_info != "none";

  for (int i = 0; i < data.aircraft_count; i++) {
    const FlightAircraft& ac = data.aircraft[i];
    Badge& badge = badges[count++];

    const float northKm = (ac.lat - config.latitude) * 111.32;
    const float eastKm = (ac.lon - config.longitude) * 111.32 * cosLat;
    badge.x = centreX + (int)(eastKm * pxPerKm);
    badge.y = centreY - (int)(northKm * pxPerKm);
    badge.dropped = false;

    String primary = badgeText(config.flight_primary_info, config, ac);
    String secondary = wantsSecondary ? badgeText(config.flight_secondary_info, config, ac) : String("");
    // When only one of the two has a value, show that one alone.
    if (wantsSecondary && (primary == "--") != (secondary == "--")) {
      if (primary == "--") primary = secondary;
      secondary = "";
    }
    if (primary.length() > 7) primary = primary.substring(0, 7);
    badge.primary = primary;
    badge.secondary = secondary;

    int textW = c.textWidth(primary, Ui::FONT_SMALL);
    int bottom = primary.length() > 0 ? 5 + LABEL_H : 4;
    if (secondary.length() > 0) {
      textW = max(textW, c.textWidth(secondary, Ui::FONT_SMALL));
      bottom = primary.length() > 0 ? bottom + 1 + LABEL_H : 5 + LABEL_H;
    }
    badge.boxW = max(textW, 7) + BADGE_GAP;
    badge.top = -4;
    badge.bottom = bottom + BADGE_GAP;
  }

  resolveOverlaps(badges, count, minX, maxX, minY, maxY);

  for (int i = 0; i < data.aircraft_count; i++) {
    const Badge& badge = badges[i + 1];
    if (badge.dropped) continue;
    Ui::Group marker(badge.primary.c_str());

    const float track = data.aircraft[i].track_deg;
    if (isnan(track)) {
      c.rect(badge.x - 1, badge.y - 1, 3, 3);
    } else {
      const Heading heading = headingFor(track);
      c.icon(heading.bitmap, badge.x - heading.w / 2, badge.y - heading.h / 2, heading.w, heading.h);
    }

    int y = badge.y + 5;
    if (badge.primary.length() > 0) {
      c.textCentered(badge.primary, badge.x, y, Ui::FONT_SMALL);
      y += LABEL_H + 1;
    }
    if (badge.secondary.length() > 0) c.textCentered(badge.secondary, badge.x, y, Ui::FONT_SMALL);
  }
}

}  // namespace

bool Screens::flightHasData(const AppState& state) {
  if (state.config.flight_mode == "closest") return state.flight.closest.callsign.length() > 0;
  return state.flight.aircraft_count > 0;
}

void Screens::drawFlight(Ui::Surface& ui, const AppState& state, int) {
  if (!flightHasData(state)) {
    ui.alert(icon_error, "No Aircraft");
    return;
  }
  if (state.config.flight_mode == "closest") drawClosest(ui, state.config, state.flight);
  else drawRadar(ui, state.config, state.flight);
}
