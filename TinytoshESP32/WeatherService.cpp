#include "WeatherService.h"

#include "HttpJson.h"
#include <ArduinoJson.h>

WeatherService::WeatherService() {}

String WeatherService::getWeatherDescription(int wmo_code) {
    if (wmo_code == 0) return "Clear Sky";
    if (wmo_code >= 1 && wmo_code <= 3) return "Cloudy";
    if (wmo_code >= 45 && wmo_code <= 48) return "Fog";
    if (wmo_code >= 51 && wmo_code <= 67) return "Rain";
    if (wmo_code >= 71 && wmo_code <= 77) return "Snow";
    if (wmo_code >= 95) return "Thunder";
    return "Unknown";
}

bool WeatherService::fetchWeather(const Config& config, WeatherData& data, const String& updateTime) {
  String url = String(WEATHER_API_BASE) + "?latitude=" + String(config.latitude, 4) +
                "&longitude=" + String(config.longitude, 4) +
                "&current=temperature_2m,weather_code,is_day" +
                "&hourly=temperature_2m,precipitation_probability" +
                "&forecast_hours=" + String(WEATHER_HOURS) +
                "&timezone=auto" +
                "&models=" + WEATHER_MODEL;

  Serial.println("WeatherService: Fetching weather data from Open-Meteo -> " + url);

  DynamicJsonDocument doc = fetchJson("WeatherService", url, 4096);
  if (doc.isNull()) return false;

  const bool fahrenheit = config.temp_unit == "F";
  auto inUnit = [fahrenheit](float celsius) { return fahrenheit ? celsius * 1.8f + 32 : celsius; };

  JsonObject cur = doc["current"];
  data.temp = inUnit(cur["temperature_2m"].as<float>());
  data.weather_code = cur["weather_code"].as<int>();
  data.is_day = cur["is_day"].as<bool>();
  data.update_time = updateTime;

  JsonArray times = doc["hourly"]["time"];
  JsonArray temps = doc["hourly"]["temperature_2m"];
  JsonArray rain = doc["hourly"]["precipitation_probability"];
  // Times read "2026-10-13T05:00".
  const String firstTime = times[0] | "";
  data.first_hour = -1;
  if ((int)temps.size() >= WEATHER_HOURS && firstTime.length() >= 13) {
    data.first_hour = firstTime.substring(11, 13).toInt();
    for (int i = 0; i < WEATHER_HOURS; i++) {
      data.hourly_temp[i] = inUnit(temps[i].as<float>());
      const int chance = rain[i] | 0;
      data.hourly_rain[i] = constrain(chance, 0, 100);
    }
  }

  Serial.printf("WeatherService: Success! Temp: %.1f%s, Code: %d\n", data.temp, config.temp_unit.c_str(), data.weather_code);
  return true;
}
