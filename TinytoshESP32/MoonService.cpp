#include "MoonService.h"

#include <ArduinoJson.h>
#include <time.h>

#include "HttpJson.h"
#include "TimeService.h"

MoonService::MoonService() {}

bool MoonService::fetchMoon(const Config& config, MoonData& data) {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char dateBuf[16];
  strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &timeinfo);
  String dateStr = String(dateBuf);
  
  struct tm utc_tm;
  gmtime_r(&now, &utc_tm);
  
  time_t local_as_utc = mktime(&utc_tm);
  float tz_offset = difftime(now, local_as_utc) / 3600.0;

  String url = String(MOON_API_BASE) + "?date=" + dateStr + "&coords=" + String(config.latitude, 4) + "," + String(config.longitude, 4) + "&tz=" + String(tz_offset, 1);

  Serial.println("MoonService: Fetching moon data from USNO -> " + url);
  
  DynamicJsonDocument doc = fetchJson("MoonService", url, 4096);
  if (doc.isNull()) return false;
  if (doc["error"].as<bool>()) {
    Serial.println("MoonService: API reported an error.");
    return false;
  }

  JsonObject dataObj = doc["properties"]["data"];
  data.curphase = dataObj["curphase"].as<String>();
  data.fracillum = dataObj["fracillum"].as<String>().toInt();
  
  data.rise_mins = -1;
  data.set_mins = -1;

  JsonArray moondata = dataObj["moondata"].as<JsonArray>();
  for (JsonObject m : moondata) {
    String phen = m["phen"].as<String>();
    if (phen == "Rise") data.rise_mins = TimeService::parseTimeToMinsFromMidnight(m["time"].as<String>(), "24");
    if (phen == "Set") data.set_mins = TimeService::parseTimeToMinsFromMidnight(m["time"].as<String>(), "24");
  }

  data.last_fetch_yday = timeinfo.tm_yday;

  Serial.printf("MoonService: Success! Phase: %s, Illum: %d%%, Rise: %d m, Set: %d m\n", data.curphase.c_str(), data.fracillum, data.rise_mins, data.set_mins);
  return true;
}
