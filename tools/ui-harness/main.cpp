// Draws every screen in every style and proves two things: nothing lands outside 128x64, and
// nothing sits closer than 2 clear pixels to the frame, a divider, or another element.
// Run: tools/ui-harness/run.sh

#include <stdio.h>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <Adafruit_GFX.h>

#include "SaverScenes.h"
#include "ScreenHost.h"
#include "Screens.h"
#include "images.h"

static const int W = Ui::SCREEN_W, H = Ui::SCREEN_H;
static const int SCALE = 3, GAP = 6, COLUMNS = 4;

static unsigned long fakeMillis = 0;
static struct tm fakeNow = {};
static String fakeClock = "12:45";

unsigned long millis() { return fakeMillis; }
unsigned long micros() { return fakeMillis * 1000; }

namespace ScreenHost {
struct tm localNow() { return fakeNow; }
String clock(const String&) { return fakeClock; }
String fullDate() { return "Wednesday, Sep 30"; }
String formatMinsFromMidnight(int mins, const String& timeFormat, bool showAmPm) {
  char buf[16];
  if (timeFormat == "24") { snprintf(buf, sizeof(buf), "%02d:%02d", mins / 60, mins % 60); return buf; }
  const int hour = (mins / 60) % 12 == 0 ? 12 : (mins / 60) % 12;
  snprintf(buf, sizeof(buf), showAmPm ? "%d:%02d %s" : "%d:%02d", hour, mins % 60, mins >= 720 ? "PM" : "AM");
  return buf;
}
String formatDurationMins(int mins) { return String(mins / 60) + "h " + String(mins % 60) + "m"; }
String weatherDescription(int wmoCode) {
  if (wmoCode == 0) return "Clear";
  if (wmoCode <= 3) return "Partly Cloudy";
  if (wmoCode >= 95) return "Thunderstorm with Hail";
  return "Rain";
}
long long livePopulation(long long base, double, int) { return base; }
}  // namespace ScreenHost

struct Element {
  Ui::Kind kind;
  std::string label;
};

// Elements drawn since the last reset. A pixel's owner is an index into this list, plus one.
static std::vector<Element> elements;
static int elementDepth = 0;
static uint16_t currentOwner = 0;

void Ui::auditBegin(Ui::Kind kind, const char* label) {
  if (elementDepth++ > 0) return;
  elements.push_back({kind, label});
  currentOwner = elements.size();
}

void Ui::auditEnd() {
  if (--elementDepth == 0) currentOwner = 0;
}

// The real driver clips silently. This one counts every write that misses the panel, and
// remembers which element lit each pixel.
class Panel : public Adafruit_GFX {
public:
  uint8_t pixels[W * H];
  uint16_t owner[W * H];
  long outOfBounds = 0;

  Panel() : Adafruit_GFX(W, H) { clear(); }
  void clear() { memset(pixels, 0, sizeof(pixels)); memset(owner, 0, sizeof(owner)); }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || x >= W || y < 0 || y >= H) { outOfBounds++; return; }
    const int i = y * W + x;
    if (color == 2) { pixels[i] = !pixels[i]; return; }
    pixels[i] = color != 0;
    owner[i] = color ? currentOwner : 0;
  }
};

static const int MIN_GAP = 2;

// Every pair of elements closer than MIN_GAP clear pixels, as readable lines. A highlight drawn
// by inverting has no owner and is not checked. Dividers and the frame may touch each other.
static std::set<std::string> spacingViolations(const Panel& panel) {
  std::map<std::pair<int, int>, int> closest;
  auto note = [&](int a, int b, int gap) {
    auto key = std::make_pair(std::min(a, b), std::max(a, b));
    auto it = closest.find(key);
    if (it == closest.end() || gap < it->second) closest[key] = gap;
  };

  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const int a = panel.owner[y * W + x];
      if (!a) continue;
      const bool contentA = elements[a - 1].kind == Ui::KIND_CONTENT;
      const int edge = std::min(std::min(x, W - 1 - x), std::min(y, H - 1 - y));
      if (contentA && edge < MIN_GAP) note(a, 0, edge);
      for (int dy = -MIN_GAP; dy <= MIN_GAP; dy++)
        for (int dx = -MIN_GAP; dx <= MIN_GAP; dx++) {
          const int qx = x + dx, qy = y + dy;
          if (qx < 0 || qx >= W || qy < 0 || qy >= H) continue;
          const int b = panel.owner[qy * W + qx];
          if (!b || b == a) continue;
          if (!contentA && elements[b - 1].kind != Ui::KIND_CONTENT) continue;
          note(a, b, std::max(abs(dx), abs(dy)) - 1);
        }
    }

  std::set<std::string> lines;
  for (const auto& entry : closest) {
    const int a = entry.first.first, b = entry.first.second;
    const std::string first = a ? elements[a - 1].label : "panel edge";
    lines.insert("'" + first + "' and '" + elements[b - 1].label + "' are " + std::to_string(entry.second) + " px apart");
  }
  return lines;
}

static void writeBmp(const std::vector<uint8_t>& gray, int w, int h, const std::string& path) {
  const int row = (w * 3 + 3) & ~3;
  uint8_t header[54] = {'B', 'M'};
  auto put32 = [&](int at, int v) { for (int i = 0; i < 4; i++) header[at + i] = (v >> (8 * i)) & 255; };
  put32(2, 54 + row * h); put32(10, 54); put32(14, 40); put32(18, w); put32(22, h);
  header[26] = 1; header[28] = 24; put32(34, row * h);
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return;
  fwrite(header, 1, 54, f);
  std::vector<uint8_t> line(row, 0);
  for (int y = h - 1; y >= 0; y--) {
    for (int x = 0; x < w; x++) line[x * 3] = line[x * 3 + 1] = line[x * 3 + 2] = gray[(size_t)y * w + x];
    fwrite(line.data(), 1, row, f);
  }
  fclose(f);
}

// Every tile as packed bits, for the review page: {"style": [{"name": ..., "bits": hex}, ...]}.
static std::string gallery;

static void addToGallery(const char* name, const Panel& panel) {
  gallery += std::string("{\"name\":\"") + name + "\",\"bits\":\"";
  for (int i = 0; i < W * H; i += 4) {
    const int nibble = panel.pixels[i] << 3 | panel.pixels[i + 1] << 2 | panel.pixels[i + 2] << 1 | panel.pixels[i + 3];
    gallery += "0123456789abcdef"[nibble];
  }
  gallery += "\"},";
}

struct Sheet {
  int tiles = 0;
  std::vector<uint8_t> gray;
  int width() const { return GAP + COLUMNS * (W * SCALE + GAP); }
  int height() const { return GAP + ((tiles + COLUMNS - 1) / COLUMNS) * (H * SCALE + GAP); }

  void add(const Panel& panel) {
    tiles++;
    gray.resize((size_t)width() * height(), 70);
    const int ox = GAP + ((tiles - 1) % COLUMNS) * (W * SCALE + GAP);
    const int oy = GAP + ((tiles - 1) / COLUMNS) * (H * SCALE + GAP);
    for (int y = 0; y < H * SCALE; y++)
      for (int x = 0; x < W * SCALE; x++)
        gray[(size_t)(oy + y) * width() + ox + x] = panel.pixels[(y / SCALE) * W + x / SCALE] ? 235 : 8;
  }

  void write(const std::string& path) const { writeBmp(gray, width(), height(), path); }
};

static AppState baseState() {
  AppState s;
  Config& c = s.config;
  c.city = "New York";
  c.country = "United States";
  c.country_code = "US";
  c.latitude = 40.7;
  c.longitude = -74.0;
  c.date_display = true;
  c.currency_multipliers[0] = 1;

  s.weather.temp = 25;
  s.weather.apparent_temperature = 26;
  s.weather.wind_speed = 12;
  s.weather.humidity = 45;
  s.weather.weather_code = 2;
  s.weather.is_day = true;
  s.weather.precipitation_probability = 10;
  s.weather.pressure = 1013;
  s.weather.visibility = 10000;

  s.aqi.aqi = 42;
  s.aqi.pm25 = 12; s.aqi.pm10 = 20; s.aqi.no2 = 8; s.aqi.co = 210; s.aqi.o3 = 61; s.aqi.uv = 3;
  s.aqi.co2 = 410; s.aqi.so2 = 2; s.aqi.dust = 5; s.aqi.ch4 = 1900;
  s.aqi.status = "Good";

  s.daylight.sunrise_mins = 372; s.daylight.sunset_mins = 1081; s.daylight.noon_mins = 726; s.daylight.length_mins = 709;
  s.moon.curphase = "Waning Crescent"; s.moon.fracillum = 22; s.moon.rise_mins = 134; s.moon.set_mins = 940;

  s.population.world_pop_base = 8267431902LL; s.population.world_growth = 0.85; s.population.world_year = 2025;
  s.population.country_pop_base = 341784857LL; s.population.country_growth = 0.49; s.population.country_year = 2025;

  s.currencies[0].base = "usd"; s.currencies[0].target = "inr"; s.currencies[0].rate = 88.42f;
  s.currencies[0].date = "2026-10-06"; s.currencies[0].updated = true;

  s.pc.cpu_percent = 37; s.pc.mem_percent = 62; s.pc.disk_percent = 81; s.pc.net_down_kb = 12700;
  s.media.status = "Playing"; s.media.name = "Bohemian Rhapsody"; s.media.author = "Queen"; s.media.album = "A Night at the Opera";

  s.bambu.status = "RUNNING"; s.bambu.progress = 62; s.bambu.time_left = 72;
  s.bambu.nozzle_temp = 220; s.bambu.nozzle_target = 220; s.bambu.bed_temp = 60; s.bambu.bed_target = 60;
  s.bambu.layer = 124; s.bambu.total_layers = 200; s.bambu.file_name = "benchy.3mf"; s.bambu.fan_part = 80; s.bambu.fan_aux = 40;

  FlightAircraft& a = s.flight.closest;
  a.callsign = "UAL456"; a.icao24 = "a1b2c3"; a.altitude_ft = 35000; a.velocity_kt = 450; a.track_deg = 270; a.distance_km = 7.8;
  a.registration = "N12345"; a.type_designator = "B738"; a.squawk = "4521"; a.origin_code = "SFO"; a.destination_code = "JFK";
  a.origin_country = "US"; a.destination_country = "US"; a.has_route = true;
  s.flight.origin_city = "San Francisco"; s.flight.destination_city = "New York"; s.flight.aircraft_manufacturer = "Boeing"; s.flight.aircraft_type = "737-800";

  const char* calls[] = {"JBU789", "FDX101", "SWA333", "UAL456", "DAL2210", "AAL9"};
  const float north[] = {0.12f, 0.16f, 0.05f, -0.10f, -0.14f, 0.01f}, east[] = {-0.55f, 0.03f, 0.50f, -0.30f, 0.40f, 0.02f};
  const float tracks[] = {180, 135, 0, 90, 315, NAN};
  s.flight.aircraft_count = 6;
  for (int i = 0; i < 6; i++) {
    FlightAircraft& r = s.flight.aircraft[i];
    r.callsign = calls[i]; r.lat = c.latitude + north[i]; r.lon = c.longitude + east[i];
    r.track_deg = tracks[i]; r.altitude_ft = 30000 + i * 1500; r.velocity_kt = 400 + i * 10; r.distance_km = 5 + i * 6;
    r.type_designator = "A320";
  }
  return s;
}

struct Scenario {
  const char* name;
  ScreenType screen;
  void (*arrange)(AppState& s);
};

static void setDate(int year, int month, int day, int weekday) {
  fakeNow = {};
  fakeNow.tm_year = year - 1900; fakeNow.tm_mon = month - 1; fakeNow.tm_mday = day; fakeNow.tm_wday = weekday;
  fakeNow.tm_hour = 12; fakeNow.tm_min = 45;
}

static const Scenario SCENARIOS[] = {
  {"time", SCREEN_TIME, [](AppState& s) { s.config.date_display = false; }},
  {"time-date", SCREEN_TIME, [](AppState&) {}},
  {"time-12h", SCREEN_TIME, [](AppState&) { fakeClock = "12:45 PM"; }},
  {"calendar", SCREEN_CALENDAR, [](AppState&) {}},
  {"calendar-6-weeks", SCREEN_CALENDAR, [](AppState&) { setDate(2026, 8, 31, 1); }},
  {"calendar-sunday", SCREEN_CALENDAR, [](AppState& s) { s.config.calendar_start_day = "sun"; setDate(2026, 9, 30, 3); }},
  {"calendar-minimal", SCREEN_CALENDAR, [](AppState& s) { s.config.calendar_minimal = true; setDate(2026, 9, 30, 3); }},
  {"weather-header", SCREEN_WEATHER, [](AppState& s) { s.config.weather_show_header = true; }},
  {"weather-header-worst", SCREEN_WEATHER, [](AppState& s) {
    s.config.weather_show_header = true; s.config.round_temps = false; s.config.city = "San Pedro de Macoris";
    s.weather.temp = -40.5f; s.weather.apparent_temperature = -48.5f; s.weather.weather_code = 99; s.weather.humidity = 100; s.weather.wind_speed = 120;
    fakeClock = "12:45 PM";
  }},
  {"weather-list-3", SCREEN_WEATHER, [](AppState&) {}},
  {"weather-list-6", SCREEN_WEATHER, [](AppState& s) {
    const char* keys[] = {"feels", "humidity", "wind", "precipitation", "pressure", "visibility"};
    for (int i = 0; i < 6; i++) s.config.weather_values[i] = keys[i];
  }},
  {"weather-list-worst", SCREEN_WEATHER, [](AppState& s) {
    const char* keys[] = {"feels", "humidity", "wind", "precipitation", "pressure", "visibility"};
    for (int i = 0; i < 6; i++) s.config.weather_values[i] = keys[i];
    s.config.round_temps = false; s.weather.temp = -40.5f; s.weather.apparent_temperature = -48.5f; s.weather.weather_code = 99;
    s.weather.humidity = 100; s.weather.wind_speed = 120; s.weather.precipitation_probability = 100; s.weather.visibility = 100000;
  }},
  {"weather-no-data", SCREEN_WEATHER, [](AppState& s) { s.weather = WeatherData(); }},
  {"aqi-header", SCREEN_AIR_QUALITY, [](AppState& s) { s.config.aqi_show_header = true; }},
  {"aqi-list-3", SCREEN_AIR_QUALITY, [](AppState&) {}},
  {"aqi-list-6", SCREEN_AIR_QUALITY, [](AppState& s) {
    const char* keys[] = {"pm25", "pm10", "no2", "co", "o3", "uv"};
    for (int i = 0; i < 6; i++) s.config.aqi_values[i] = keys[i];
  }},
  {"aqi-worst", SCREEN_AIR_QUALITY, [](AppState& s) {
    const char* keys[] = {"pm25", "pm10", "co2", "co", "ch4", "dust"};
    for (int i = 0; i < 6; i++) s.config.aqi_values[i] = keys[i];
    s.config.aqi_type = "EU"; s.aqi.aqi = 500; s.aqi.status = "Unhealthy for Sensitive Groups";
    s.aqi.pm25 = 999; s.aqi.pm10 = 999; s.aqi.co = 9999; s.aqi.dust = 999;
  }},
  {"daylight", SCREEN_DAYLIGHT, [](AppState&) {}},
  {"daylight-12h-night", SCREEN_DAYLIGHT, [](AppState& s) { s.config.time_format = "12"; fakeNow.tm_hour = 23; }},
  {"daylight-minimal", SCREEN_DAYLIGHT, [](AppState& s) { s.config.daylight_minimal = true; }},
  {"moon", SCREEN_MOON, [](AppState&) {}},
  {"moon-waxing-12h", SCREEN_MOON, [](AppState& s) { s.config.time_format = "12"; s.moon.curphase = "Waxing Gibbous"; s.moon.fracillum = 78; s.moon.rise_mins = 700; s.moon.set_mins = 1380; }},
  {"moon-minimal", SCREEN_MOON, [](AppState& s) { s.config.moon_minimal = true; }},
  {"population", SCREEN_POPULATION, [](AppState&) {}},
  {"population-long-name", SCREEN_POPULATION, [](AppState& s) { s.config.country = "Saint Vincent and the Grenadines"; s.population.country_growth = -1.25; }},
  {"population-country", SCREEN_POPULATION, [](AppState& s) { s.config.pop_show_world = false; s.config.country = "Saint Vincent and the Grenadines"; }},
  {"flight-closest", SCREEN_FLIGHT, [](AppState&) {}},
  {"flight-closest-metric", SCREEN_FLIGHT, [](AppState& s) {
    s.config.flight_units = "metric"; s.flight.origin_city = "Thiruvananthapuram"; s.flight.destination_city = "S\xC3\xA3o Paulo Guarulhos";
    s.flight.closest.velocity_kt = 520; s.flight.closest.distance_km = 148; s.flight.aircraft_manufacturer = "McDonnell Douglas";
  }},
  {"flight-radar", SCREEN_FLIGHT, [](AppState& s) { s.config.flight_mode = "radar"; }},
  {"flight-radar-two-lines", SCREEN_FLIGHT, [](AppState& s) { s.config.flight_mode = "radar"; s.config.flight_secondary_info = "altitude"; }},
  {"flight-none", SCREEN_FLIGHT, [](AppState& s) { s.flight.closest.callsign = ""; }},
  {"currency", SCREEN_CURRENCY, [](AppState&) {}},
  {"currency-large", SCREEN_CURRENCY, [](AppState& s) { s.currencies[0].base = "bam"; s.currencies[0].target = "idr"; s.currencies[0].rate = 9617.4f; s.config.currency_multipliers[0] = 1000; }},
  {"pc", SCREEN_PC_MONITOR, [](AppState&) {}},
  {"pc-maxed", SCREEN_PC_MONITOR, [](AppState& s) { s.pc.cpu_percent = 100; s.pc.mem_percent = 100; s.pc.disk_percent = 100; s.pc.net_down_kb = 999999; }},
  {"pc-none", SCREEN_PC_MONITOR, [](AppState& s) { s.pc.cpu_percent = 0; s.pc.mem_percent = 0; }},
  {"media", SCREEN_PC_MEDIA, [](AppState&) {}},
  {"media-long", SCREEN_PC_MEDIA, [](AppState& s) {
    s.media.status = "Paused"; s.media.name = "Supercalifragilisticexpialidocious and other very long words";
    s.media.author = "The Mary Poppins Original Cast"; s.media.album = "Walt Disney Records The Legacy Collection Deluxe";
  }},
  {"media-no-album", SCREEN_PC_MEDIA, [](AppState& s) { s.media.album = ""; s.media.name = "A song with a fairly long name that wraps to three lines"; }},
  {"bambu-printing", SCREEN_BAMBU, [](AppState&) {}},
  {"bambu-long-file", SCREEN_BAMBU, [](AppState& s) { s.bambu.file_name = "articulated_dragon_v12_final_supports.gcode.3mf"; s.bambu.progress = 100; s.bambu.time_left = 1440; s.bambu.layer = 1999; s.bambu.total_layers = 2000; s.bambu.fan_part = 100; s.bambu.fan_aux = 100; s.bambu.nozzle_temp = 300; s.bambu.nozzle_target = 300; s.bambu.bed_temp = 110; s.bambu.bed_target = 110; }},
  {"bambu-idle", SCREEN_BAMBU, [](AppState& s) { s.bambu.status = "FINISHED"; s.bambu.nozzle_temp = 215; s.bambu.bed_temp = 110; }},
  {"bambu-none", SCREEN_BAMBU, [](AppState& s) { s.bambu.status = "SYNCING"; }},
  {"daylight-no-data", SCREEN_DAYLIGHT, [](AppState& s) { s.daylight = DaylightData(); }},
};

// Screens outside the rotation: alerts, the boot card, and notices.
struct Card {
  const char* name;
  void (*draw)(Ui::Surface& ui);
};

static const Card CARDS[] = {
  {"alert", [](Ui::Surface& ui) { ui.alert(icon_lock, "Auto Cycle Off"); }},
  {"startup-welcome", [](Ui::Surface& ui) { ui.startup(icon_mac_happy, "Welcome to Tinytosh", "Starting up", 5); }},
  {"startup-longest", [](Ui::Surface& ui) { ui.startup(icon_mac_happy, "Updating Air Quality", "America/Argentina/ComodRivadavia", 44); }},
  {"startup-ready", [](Ui::Surface& ui) { ui.startup(icon_mac_happy, "Tinytosh is Ready", "Welcome", 100); }},
  {"startup-failed", [](Ui::Surface& ui) { ui.startup(icon_mac_sad, "Connect Failed", "Use the Web Panel to set WiFi", -1); }},
  {"notice-wifi", [](Ui::Surface& ui) {
    const String lines[] = {"Join this network:", "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345", "", "Password:", "Tinytosh"};
    ui.notice("WiFi Setup", lines, 5);
  }},
  {"notice-connected", [](Ui::Surface& ui) {
    const String lines[] = {"Web Panel:", "192.168.100.200", "tinytosh-ab12.local"};
    ui.notice("Connected", lines, 3);
  }},
};

// The setup guide's pictures, drawn from the real screens so they cannot drift from the firmware.
struct Render {
  const char* file;
  const char* scenario;
};

static const Render RENDERS[] = {
  {"render_clock", "time-date"}, {"render_calendar", "calendar"},
  {"render_weather", "weather-header"}, {"render_weather_headless", "weather-list-3"},
  {"render_aqi", "aqi-header"}, {"render_aqi_headless", "aqi-list-3"},
  {"render_daylight", "daylight"}, {"render_moon", "moon"}, {"render_population", "population"},
  {"render_flight_radar", "flight-radar"}, {"render_flight_closest", "flight-closest"},
  {"render_currency", "currency"}, {"render_pc", "pc"}, {"render_media", "media"},
  {"render_bambu_idle", "bambu-idle"}, {"render_bambu_active", "bambu-printing"},
};

struct StyleCase {
  const char* tag;
  Ui::Style style;
};

static long auditSpacing() {
  const StyleCase chromes[] = {{"window", {Ui::CHROME_WINDOW, false}}, {"menubar", {Ui::CHROME_MENUBAR, false}}};
  long total = 0;
  auto check = [&](const Panel& panel, const char* chrome, const char* name) {
    for (const std::string& line : spacingViolations(panel)) {
      printf("  %-8s %-24s %s\n", chrome, name, line.c_str());
      total++;
    }
  };

  for (const StyleCase& sc : chromes) {
    for (const Scenario& scenario : SCENARIOS) {
      AppState state = baseState();
      fakeClock = "12:45";
      setDate(2026, 10, 7, 3);
      scenario.arrange(state);
      elements.clear();
      Panel panel;
      Ui::Surface ui(panel, sc.style, fakeClock);
      SCREENS[scenario.screen].draw(ui, state, 0);
      check(panel, sc.tag, scenario.name);
    }
    for (const Card& card : CARDS) {
      elements.clear();
      Panel panel;
      Ui::Surface ui(panel, sc.style, fakeClock);
      card.draw(ui);
      check(panel, sc.tag, card.name);
    }
  }
  printf("spacing        %ld pairs closer than %d px\n", total, MIN_GAP);
  return total;
}

static long drawScreens() {
  const StyleCase styles[] = {
    {"window-black", {Ui::CHROME_WINDOW, false}}, {"window-white", {Ui::CHROME_WINDOW, true}},
    {"menubar-black", {Ui::CHROME_MENUBAR, false}}, {"menubar-white", {Ui::CHROME_MENUBAR, true}},
  };
  long total = 0;
  gallery = "{";
  for (const StyleCase& sc : styles) {
    Sheet sheet;
    gallery += std::string("\"") + sc.tag + "\":[";
    for (const Scenario& scenario : SCENARIOS) {
      AppState state = baseState();
      fakeClock = "12:45";
      setDate(2026, 10, 7, 3);
      scenario.arrange(state);

      Panel panel;
      Ui::Surface ui(panel, sc.style, fakeClock);
      SCREENS[scenario.screen].draw(ui, state, 0);
      ui.finish();

      if (panel.outOfBounds) printf("  %-14s %-26s out-of-bounds=%ld\n", sc.tag, scenario.name, panel.outOfBounds);
      total += panel.outOfBounds;
      sheet.add(panel);
      addToGallery(scenario.name, panel);
    }

    for (const Card& card : CARDS) {
      Panel panel;
      Ui::Surface ui(panel, sc.style, fakeClock);
      card.draw(ui);
      ui.finish();
      if (panel.outOfBounds) printf("  %-14s %-26s out-of-bounds=%ld\n", sc.tag, card.name, panel.outOfBounds);
      total += panel.outOfBounds;
      sheet.add(panel);
      addToGallery(card.name, panel);
    }
    gallery.back() = ']';
    gallery += ",";

    sheet.write(std::string("out/screens-") + sc.tag + ".bmp");
    printf("%-14s %d tiles\n", sc.tag, sheet.tiles);
  }
  gallery.back() = '}';
  if (FILE* f = fopen("out/screens.json", "w")) { fputs(gallery.c_str(), f); fclose(f); }
  return total;
}

static bool writeRenders() {
  const int scale = 7, width = 952, height = 500;
  const int ox = (width - W * scale) / 2, oy = (height - H * scale) / 2;
  for (const Render& render : RENDERS) {
    const Scenario* found = nullptr;
    for (const Scenario& scenario : SCENARIOS) {
      if (std::string(scenario.name) == render.scenario) found = &scenario;
    }
    if (!found) { printf("FAIL: render %s names unknown scenario %s\n", render.file, render.scenario); return false; }

    AppState state = baseState();
    fakeClock = "12:45";
    setDate(2026, 10, 7, 3);
    found->arrange(state);
    Panel panel;
    Ui::Surface ui(panel, Ui::Style(), fakeClock);
    SCREENS[found->screen].draw(ui, state, 0);
    ui.finish();

    std::vector<uint8_t> gray((size_t)width * height, 0);
    for (int x = ox - 4; x < ox + W * scale + 4; x++) gray[(size_t)(oy - 4) * width + x] = gray[(size_t)(oy + H * scale + 3) * width + x] = 45;
    for (int y = oy - 4; y < oy + H * scale + 4; y++) gray[(size_t)y * width + ox - 4] = gray[(size_t)y * width + ox + W * scale + 3] = 45;
    for (int y = 0; y < H * scale; y++)
      for (int x = 0; x < W * scale; x++)
        if (panel.pixels[(y / scale) * W + x / scale]) gray[(size_t)(oy + y) * width + ox + x] = 255;
    writeBmp(gray, width, height, std::string("out/renders/") + render.file + ".bmp");
  }
  printf("renders        %d pictures\n", (int)(sizeof(RENDERS) / sizeof(RENDERS[0])));
  return true;
}

static long runScene(int index, bool hasWeather, int temp, int code, int frames) {
  Panel panel;
  const SaverScene& scene = SAVER_SCENES[index];
  scene.init(0xC0FFEE + index);
  for (int f = 0; f < frames; f++) {
    panel.clear();
    SaverInput in{(uint32_t)f * 40, f ? 40u : 0u, hasWeather, temp, code};
    scene.draw(panel, in);
  }
  return panel.outOfBounds;
}

static long runClock(const char* hhmm, const char* title) {
  Panel panel;
  for (int f = 0; f < 4 * 12000 / 40; f++) {
    panel.clear();
    drawDesktopClock(panel, hhmm, title, (uint32_t)f * 40);
  }
  return panel.outOfBounds;
}

static long drawAnimations() {
  long total = runScene(0, false, 0, 0, 3 * 16000 / 40) + runScene(1, false, 0, 0, 6000) + runScene(2, false, 0, 0, 6000);
  const int codes[] = {0, 1, 2, 3, 45, 48, 51, 55, 61, 65, 67, 71, 75, 77, 80, 82, 85, 86, 95, 96, 99};
  const int temps[] = {18, 0, -5, -40, 104, -104, 9999, -999};
  for (int code : codes)
    for (int temp : temps) total += runScene(3, true, temp, code, 1500);
  total += runScene(3, false, 0, 0, 1500);
  total += runClock("23:59", "CLOCK") + runClock("12:00", "Wednesday, Sep 30") + runClock("08:08", "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWW") + runClock("--:--", "No Date");
  printf("animations     saver scenes and desktop clock\n");
  return total;
}

int main() {
  for (int i = 0; i < NUM_SCREENS; i++) {
    if (SCREENS[i].id != i) { printf("FAIL: SCREENS row %d holds screen id %d\n", i, SCREENS[i].id); return 1; }
  }
  if (!writeRenders()) return 1;
  const long outOfBounds = drawScreens() + drawAnimations();
  const long crowded = auditSpacing();
  const bool pass = outOfBounds == 0 && crowded == 0;
  printf("\n%s: %ld out-of-bounds pixel writes, %ld spacing violations\n", pass ? "PASS" : "FAIL", outOfBounds, crowded);
  return pass ? 0 : 1;
}
