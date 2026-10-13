#include "JsonSerializer.h"

#include "PopulationService.h"
#include "TimeService.h"

namespace {

// One row per setting that is a single value: where it sits in the JSON and which Config member
// holds it. Writing and reading both walk this table, so a new setting is one new row. Settings
// that are lists (screen order, extra values, currency pairs) are handled by hand below.
struct ConfigField {
    enum Kind { INT, FLAG, TEXT, REAL, ULONG, U16 };

    const char* group;
    const char* key;
    Kind kind;
    union {
        int Config::* asInt;
        bool Config::* asFlag;
        String Config::* asText;
        float Config::* asReal;
        unsigned long Config::* asUlong;
        uint16_t Config::* asU16;
    };

    constexpr ConfigField(const char* g, const char* k, int Config::* m) : group(g), key(k), kind(INT), asInt(m) {}
    constexpr ConfigField(const char* g, const char* k, bool Config::* m) : group(g), key(k), kind(FLAG), asFlag(m) {}
    constexpr ConfigField(const char* g, const char* k, String Config::* m) : group(g), key(k), kind(TEXT), asText(m) {}
    constexpr ConfigField(const char* g, const char* k, float Config::* m) : group(g), key(k), kind(REAL), asReal(m) {}
    constexpr ConfigField(const char* g, const char* k, unsigned long Config::* m) : group(g), key(k), kind(ULONG), asUlong(m) {}
    constexpr ConfigField(const char* g, const char* k, uint16_t Config::* m) : group(g), key(k), kind(U16), asU16(m) {}
};

const ConfigField CONFIG_FIELDS[] = {
    {"hardware", "sda_pin", &Config::sda_pin},
    {"hardware", "scl_pin", &Config::scl_pin},
    {"hardware", "button_pin", &Config::button_pin},
    {"hardware", "button_type", &Config::button_type},
    {"hardware", "presence_ble", &Config::presence_ble},

    {"general", "refresh_min", &Config::refresh_interval_min},
    {"general", "time_format", &Config::time_format},
    {"general", "auto_detect", &Config::auto_detect},
    {"general", "latitude", &Config::latitude},
    {"general", "longitude", &Config::longitude},
    {"general", "country", &Config::country},
    {"general", "country_code", &Config::country_code},
    {"general", "city", &Config::city},
    {"general", "timezone", &Config::timezone},
    {"general", "ntp_server", &Config::ntp_server},
    {"general", "date_display", &Config::date_display},
    {"general", "time_style", &Config::time_style},
    {"general", "ui_chrome", &Config::ui_chrome},
    {"general", "ui_paper", &Config::ui_paper},

    {"theme", "bg", &Config::theme_bg},
    {"theme", "card", &Config::theme_card},
    {"theme", "accent", &Config::theme_accent},
    {"theme", "text", &Config::theme_text},

    {"night", "mode", &Config::night_mode},
    {"night", "start", &Config::night_start},
    {"night", "end", &Config::night_end},
    {"night", "action", &Config::night_action},
    {"night", "dim_start", &Config::night_dim_start},

    {"screens", "auto_cycle", &Config::screen_auto_cycle},
    {"screens", "interval_sec", &Config::screen_interval_sec},
    {"screens", "anim_mask", &Config::anim_mask},
    {"screens", "saver_mask", &Config::saver_mask},
    {"screens", "show_time", &Config::show_time},
    {"screens", "show_calendar", &Config::show_calendar},
    {"screens", "show_weather", &Config::show_weather},
    {"screens", "show_aqi", &Config::show_aqi},
    {"screens", "show_daylight", &Config::show_daylight},
    {"screens", "show_moon", &Config::show_moon},
    {"screens", "show_population", &Config::show_population},
    {"screens", "show_pc", &Config::show_pc},
    {"screens", "show_media", &Config::show_media},
    {"screens", "show_currency", &Config::show_currency},
    {"screens", "show_bambu", &Config::show_bambu},
    {"screens", "show_flight", &Config::show_flight},
    {"screens", "show_saver", &Config::show_saver},
    {"screens", "hide_empty_pc", &Config::hide_empty_pc},
    {"screens", "hide_empty_media", &Config::hide_empty_media},
    {"screens", "hide_empty_bambu", &Config::hide_empty_bambu},
    {"screens", "hide_empty_flight", &Config::hide_empty_flight},

    {"calendar", "start_day", &Config::calendar_start_day},
    {"calendar", "minimal", &Config::calendar_minimal},

    {"weather", "temp_unit", &Config::temp_unit},
    {"weather", "round_temps", &Config::round_temps},
    {"weather", "show_header", &Config::weather_show_header},
    {"weather", "custom_sync_min", &Config::custom_weather_int_min},

    {"aqi", "type", &Config::aqi_type},
    {"aqi", "show_header", &Config::aqi_show_header},
    {"aqi", "custom_sync_min", &Config::custom_aqi_int_min},

    {"daylight", "minimal", &Config::daylight_minimal},
    {"moon", "minimal", &Config::moon_minimal},

    {"population", "show_world", &Config::pop_show_world},
    {"population", "show_country", &Config::pop_show_country},

    {"currency", "fn", &Config::currency_fn},
    {"currency", "custom_sync_min", &Config::custom_currency_int_min},

    {"printer", "ip", &Config::bambu_ip},
    {"printer", "sn", &Config::bambu_sn},
    {"printer", "code", &Config::bambu_code},

    {"flight", "mode", &Config::flight_mode},
    {"flight", "radius_nm", &Config::flight_radius_nm},
    {"flight", "units", &Config::flight_units},
    {"flight", "primary_info", &Config::flight_primary_info},
    {"flight", "secondary_info", &Config::flight_secondary_info},
    {"flight", "custom_sync_min", &Config::custom_flight_int_min},
};

void writeField(const ConfigField& field, const Config& config, JsonObject group) {
    switch (field.kind) {
        case ConfigField::INT: group[field.key] = config.*field.asInt; break;
        case ConfigField::FLAG: group[field.key] = config.*field.asFlag ? 1 : 0; break;
        case ConfigField::TEXT: group[field.key] = config.*field.asText; break;
        case ConfigField::REAL: group[field.key] = config.*field.asReal; break;
        case ConfigField::ULONG: group[field.key] = config.*field.asUlong; break;
        case ConfigField::U16: group[field.key] = config.*field.asU16; break;
    }
}

void readField(const ConfigField& field, Config& config, JsonVariantConst value) {
    switch (field.kind) {
        case ConfigField::INT: config.*field.asInt = value; break;
        case ConfigField::FLAG: config.*field.asFlag = value == 1; break;
        case ConfigField::TEXT: config.*field.asText = value.as<String>(); break;
        case ConfigField::REAL: config.*field.asReal = value.as<float>(); break;
        case ConfigField::ULONG: config.*field.asUlong = value; break;
        case ConfigField::U16: config.*field.asU16 = value; break;
    }
}

template <size_t N>
void writeValues(JsonObject group, const String (&values)[N]) {
    JsonArray array = group.createNestedArray("values");
    for (const String& value : values) if (value.length() > 0) array.add(value);
}

template <size_t N>
void readValues(JsonObjectConst group, String (&values)[N]) {
    if (!group.containsKey("values")) return;
    for (String& value : values) value = "";
    size_t i = 0;
    for (JsonVariantConst v : group["values"].as<JsonArrayConst>()) if (i < N) values[i++] = v.as<String>();
}

}  // namespace

void JsonSerializer::populateConfigDoc(const Config& config, JsonObject configObj) {
    JsonObject network = configObj.createNestedObject("network");
    network["device_id"] = config.device_id;
    network["ip_address"] = config.ip_address;

    for (const ConfigField& field : CONFIG_FIELDS) {
        JsonObject group = configObj[field.group];
        if (group.isNull()) group = configObj.createNestedObject(field.group);
        writeField(field, config, group);
    }

    String orderStr = "";
    for (int i = 0; i < NUM_SCREENS; i++) {
        orderStr += String(config.screen_order[i]);
        if (i < NUM_SCREENS - 1) orderStr += ",";
    }
    configObj["screens"]["order"] = orderStr;

    writeValues(configObj["weather"], config.weather_values);
    writeValues(configObj["aqi"], config.aqi_values);

    JsonObject currency = configObj["currency"];
    JsonArray cbArr = currency.createNestedArray("bases");
    for (int i = 0; i < config.currency_count; i++) cbArr.add(config.currency_bases[i]);
    JsonArray ctArr = currency.createNestedArray("targets");
    for (int i = 0; i < config.currency_count; i++) ctArr.add(config.currency_targets[i]);
    JsonArray cmArr = currency.createNestedArray("multipliers");
    for (int i = 0; i < config.currency_count; i++) cmArr.add(config.currency_multipliers[i]);
}

String JsonSerializer::buildConfigJson(const Config& config) {
    DynamicJsonDocument doc(3072);
    JsonObject root = doc.to<JsonObject>();
    populateConfigDoc(config, root);
    String output;
    serializeJson(doc, output);
    return output;
}

String JsonSerializer::buildAppStateJson(const AppState& state) {
    DynamicJsonDocument doc(6144);

    // 1. Config, grouped identically to buildConfigJson's output
    JsonObject configObj = doc.createNestedObject("config");
    populateConfigDoc(state.config, configObj);

    // 2. Live status, grouped by the same domains as config
    JsonObject status = doc.createNestedObject("status");

    JsonObject general = status.createNestedObject("general");
    general["time"] = TimeService::getCurrentTimeShort(state.config.time_format);
    general["date"] = TimeService::getFullDate();

    // What tools/device-stress.sh watches: a reboot resets uptime, a leak lowers the heap floor.
    JsonObject system = status.createNestedObject("system");
    system["uptime_s"] = millis() / 1000;
    system["free_heap"] = ESP.getFreeHeap();
    system["min_free_heap"] = ESP.getMinFreeHeap();
    system["largest_block"] = ESP.getMaxAllocHeap();
    system["loop_stack_free"] = uxTaskGetStackHighWaterMark(NULL);

    JsonObject weather = status.createNestedObject("weather");
    weather["update_time"] = state.weather.update_time;
    if (!isnan(state.weather.temp)) {
        weather["temp"] = String(state.weather.temp, 1);
        weather["apparent_temperature"] = String(state.weather.apparent_temperature, 1);
        weather["humidity"] = String(state.weather.humidity);
        weather["wind_speed"] = String(state.weather.wind_speed, 1);
        weather["weather_code"] = state.weather.weather_code;
        weather["precipitation_probability"] = String(state.weather.precipitation_probability, 0);
        weather["pressure"] = String(state.weather.pressure, 0);
        weather["visibility"] = String(state.weather.visibility / 1000.0, 1);
    }

    if (state.aqi.aqi != -1) {
        JsonObject aqi = status.createNestedObject("aqi");
        aqi["index"] = String(state.aqi.aqi);
        aqi["status"] = state.aqi.status;
        aqi["pm25"] = String(state.aqi.pm25, 1);
        aqi["pm10"] = String(state.aqi.pm10, 1);
        aqi["no2"] = String(state.aqi.no2, 1);
        aqi["co"] = String(state.aqi.co, 1);
        aqi["co2"] = String(state.aqi.co2, 1);
        aqi["so2"] = String(state.aqi.so2, 1);
        aqi["o3"] = String(state.aqi.o3, 1);
        aqi["dust"] = String(state.aqi.dust, 1);
        aqi["uv"] = String(state.aqi.uv, 1);
        aqi["ch4"] = String(state.aqi.ch4, 1);
    }

    if (state.daylight.sunrise_mins != -1) {
        JsonObject daylight = status.createNestedObject("daylight");
        daylight["sunrise"] = TimeService::formatMinsFromMidnight(state.daylight.sunrise_mins, state.config.time_format);
        daylight["sunset"] = TimeService::formatMinsFromMidnight(state.daylight.sunset_mins, state.config.time_format);
        daylight["solar_noon"] = TimeService::formatMinsFromMidnight(state.daylight.noon_mins, state.config.time_format);
        daylight["day_length"] = TimeService::formatDurationMins(state.daylight.length_mins);
    }

    if (state.moon.curphase != "N/A") {
        JsonObject moon = status.createNestedObject("moon");
        moon["rise"] = state.moon.rise_mins != -1 ? TimeService::formatMinsFromMidnight(state.moon.rise_mins, state.config.time_format) : "--:--";
        moon["set"] = state.moon.set_mins != -1 ? TimeService::formatMinsFromMidnight(state.moon.set_mins, state.config.time_format) : "--:--";
        moon["phase"] = state.moon.curphase;
        moon["illum"] = state.moon.fracillum;
    }

    if ((state.config.pop_show_world && state.population.world_pop_base != -1) ||
        (state.config.pop_show_country && state.population.country_pop_base != -1)) {
        JsonObject population = status.createNestedObject("population");
        if (state.config.pop_show_world && state.population.world_pop_base != -1) {
            population["world_live"] = String(PopulationService::getLivePopulation(state.population.world_pop_base, state.population.world_growth, state.population.world_year));
            population["world_growth"] = String(state.population.world_growth, 2);
        }
        if (state.config.pop_show_country && state.population.country_pop_base != -1) {
            population["country_live"] = String(PopulationService::getLivePopulation(state.population.country_pop_base, state.population.country_growth, state.population.country_year));
            population["country_growth"] = String(state.population.country_growth, 2);
        }
    }

    JsonObject currency = status.createNestedObject("currency");
    JsonArray currencyData = currency.createNestedArray("data");
    for (int i = 0; i < state.config.currency_count; i++) {
        if (state.currencies[i].updated) {
            JsonObject obj = currencyData.createNestedObject();
            float displayRate = state.currencies[i].rate * state.config.currency_multipliers[i];
            int decimals = (displayRate < 10.0) ? 3 : (displayRate < 100.0) ? 2 : (displayRate < 1000.0) ? 1 : 0;
            obj["base_text"] = String(state.config.currency_multipliers[i]) + " " + state.currencies[i].base;
            obj["target_text"] = String(displayRate, decimals) + " " + state.currencies[i].target;
        }
    }

    JsonObject pc = status.createNestedObject("pc");
    if (state.pc.cpu_percent > 0.1) {
        pc["cpu"] = String(state.pc.cpu_percent);
        pc["net"] = String(state.pc.net_down_kb);
        pc["ram"] = String(state.pc.mem_percent);
        pc["disk"] = String(state.pc.disk_percent);
    }
    String activeId = state.config.active_pc_id;
    int lastColonPos = activeId.lastIndexOf(':');
    if (lastColonPos > 3) activeId = activeId.substring(0, lastColonPos);
    bool isPcPaired = (activeId != "" && (millis() - state.pc.last_update < 10000));
    pc["status"] = isPcPaired ? ("🔒 Paired to " + activeId) : "";

    if (state.media.status.length() > 0) {
        JsonObject media = status.createNestedObject("media");
        media["status"] = state.media.status;
        media["name"] = state.media.name;
        media["author"] = state.media.author;
        media["album"] = state.media.album;
    }

    if (state.bambu.status != "SYNCING") {
        JsonObject printer = status.createNestedObject("printer");
        printer["status"] = state.bambu.status;
        printer["progress"] = state.bambu.progress;
        printer["time"] = state.bambu.time_left;
        printer["layer"] = state.bambu.layer;
        printer["total_layers"] = state.bambu.total_layers;
        printer["nozzle"] = String(state.bambu.nozzle_temp, 1);
        printer["nozzle_target"] = String(state.bambu.nozzle_target, 1);
        printer["bed"] = String(state.bambu.bed_temp, 1);
        printer["bed_target"] = String(state.bambu.bed_target, 1);
        printer["fan_part"] = state.bambu.fan_part;
        printer["fan_aux"] = state.bambu.fan_aux;
    }

    bool flightValid = (state.config.flight_mode == "closest") ? (state.flight.closest.callsign.length() > 0) : (state.flight.aircraft_count > 0);
    if (flightValid) {
        const FlightAircraft& c = (state.config.flight_mode == "closest") ? state.flight.closest : state.flight.aircraft[0];
        JsonObject flight = status.createNestedObject("flight");
        flight["count"] = state.flight.aircraft_count;
        flight["callsign"] = c.callsign;
        if (state.config.flight_mode == "closest") {
            flight["route"] = c.has_route ? (c.origin_code + " &rarr; " + c.destination_code) : "N/A";
        }
    }

    String output;
    serializeJson(doc, output);
    return output;
}

bool JsonSerializer::parseConfig(const char* jsonString, AppState& state) {
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) return false;

    Config& config = state.config;

    for (const ConfigField& field : CONFIG_FIELDS) {
        JsonObjectConst group = doc[field.group];
        if (group.containsKey(field.key)) readField(field, config, group[field.key]);
    }

    config.ntp_server.trim();
    if (doc["general"].containsKey("country_code")) {
        for (auto c : allCountries) {
            if (config.country_code == String(c.code)) {
                config.country = c.name;
                break;
            }
        }
    }

    JsonObjectConst screens = doc["screens"];
    if (screens.containsKey("order")) {
        String orderStr = screens["order"].as<String>();
        // Configs saved by older firmware (marked by show_stock) hold 14 ids with Stock=8 and Crypto=9; drop them and shift the rest down.
        bool legacy = screens.containsKey("show_stock");
        int idx = 0; int startPos = 0;
        while (startPos < orderStr.length() && (legacy || idx < NUM_SCREENS)) {
            int commaPos = orderStr.indexOf(',', startPos);
            int id = (commaPos == -1 ? orderStr.substring(startPos) : orderStr.substring(startPos, commaPos)).toInt();
            if (!legacy || (id != 8 && id != 9)) {
                if (legacy && id > 9) id -= 2;
                if (idx < NUM_SCREENS) config.screen_order[idx++] = id;
            }
            if (commaPos == -1) break;
            startPos = commaPos + 1;
        }
        // An order saved before a screen existed lacks its id; append it so the screen appears after an upgrade.
        for (int id = 0; id < NUM_SCREENS && idx < NUM_SCREENS; id++) {
            bool present = false;
            for (int j = 0; j < idx; j++) if (config.screen_order[j] == id) present = true;
            if (!present) config.screen_order[idx++] = id;
        }
    }

    readValues(doc["weather"], config.weather_values);
    readValues(doc["aqi"], config.aqi_values);

    JsonObjectConst currency = doc["currency"];
    if (currency.containsKey("bases")) {
        JsonArrayConst arrB = currency["bases"];
        JsonArrayConst arrT = currency["targets"];
        JsonArrayConst arrM = currency["multipliers"];
        config.currency_count = 0;
        for (int i = 0; i < arrB.size(); i++) {
            if (config.currency_count < MAX_MULTI_ENTRIES) {
                config.currency_bases[config.currency_count] = arrB[i].as<String>();
                config.currency_targets[config.currency_count] = arrT[i].as<String>();
                config.currency_multipliers[config.currency_count] = arrM[i].as<int>();
                config.currency_count++;
            }
        }
        if (config.currency_count == 0) { config.currency_bases[0] = "usd"; config.currency_targets[0] = "eur"; config.currency_multipliers[0] = 1; config.currency_count = 1; }
    }

    // Dynamic State Wipes (Triggers data reload)
    state.daylight.last_fetch_yday = -1;

    state.moon.last_fetch_yday = -1;
    state.population.last_fetch_yday = -1;

    if (!config.show_weather) { state.weather.temp = NAN; state.weather.humidity = 0; state.weather.apparent_temperature = NAN; state.weather.wind_speed = NAN; }
    if (!config.show_aqi) state.aqi = AirQualityData();
    if (!config.show_daylight) state.daylight = DaylightData();
    if (!config.show_moon) state.moon = MoonData();
    if (!config.show_population) state.population = PopulationData();
    if (!config.show_pc) { state.pc.cpu_percent = 0; state.pc.net_down_kb = 0; state.pc.mem_percent = 0; state.pc.disk_percent = 0; }
    if (!config.show_flight) state.flight = FlightData();

    // Array Wipes
    if (!config.show_currency) {
        for (int i = 0; i < MAX_MULTI_ENTRIES; i++) { state.currencies[i].rate = NAN; state.currencies[i].updated = false; }
    }

    if (!config.show_media) { state.media.status = "stopped"; state.media.name = ""; }

    return true;
}


bool JsonSerializer::parsePresence(JsonVariantConst stats, AppState& state) {
    JsonObjectConst p = stats["presence"];
    if (p.isNull()) return false;

    Presence::Command& command = state.presence_command;
    command.session = p["session"] | 0;
    command.lease = p["lease"] | false;
    command.lease_ms = p["lease_ms"] | 0;
    command.paused = p["paused"] | false;
    command.profile = p["profile"] | (int)Presence::PROFILE_BALANCED;
    command.trigger_seq = p["trigger_seq"] | 0;
    command.reset_seq = p["reset_seq"] | 0;
    command.host_asleep = p["host_asleep"] | false;
    state.presence_command_fresh = true;
    return true;
}

String JsonSerializer::buildPresenceJson(const Presence::Status& status) {
    static const char* const LINKS[] = {"off", "advertising", "connected"};
    StaticJsonDocument<256> doc;
    doc["link"] = LINKS[status.link];
    doc["paused"] = status.paused;
    doc["leased"] = status.leased;
    doc["moving"] = status.moving;
    doc["host_asleep"] = status.host_asleep;
    doc["profile"] = status.profile;
    doc["last_gesture"] = status.last_gesture;
    doc["next_sec"] = status.next_sec;
    doc["pos_x"] = status.pos_x;
    doc["pos_y"] = status.pos_y;
    doc["count"] = status.count;
    String output;
    serializeJson(doc, output);
    return output;
}
