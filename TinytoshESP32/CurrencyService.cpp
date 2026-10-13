#include "CurrencyService.h"

#include <ArduinoJson.h>
#include "HttpJson.h"

bool CurrencyService::fetchRate(const String& base, const String& target, CurrencyData &data) {
    String safeBase = base;
    safeBase.toLowerCase();
    safeBase.trim();
    
    String safeTarget = target;
    safeTarget.toLowerCase();
    safeTarget.trim();

    DynamicJsonDocument filter(256);
    filter["date"] = true;
    filter[safeBase.c_str()][safeTarget.c_str()] = true;
    HttpJsonOptions options;
    options.filter = &filter;

    for (int i = 0; i < 2; i++) {
        String url = String(CURRENCY_API_URLS[i]) + safeBase + ".min.json";
        Serial.println("CurrencyService: Fetching currency from API (Attempt " + String(i + 1) + ") -> " + url);

        DynamicJsonDocument doc = fetchJson("CurrencyService", url, 1024, options);
        if (!doc.isNull() && doc.containsKey("date") && doc.containsKey(safeBase)) {
            data.base = safeBase;
            data.base.toUpperCase();
            data.target = safeTarget;
            data.target.toUpperCase();
            data.date = doc["date"].as<String>();
            data.rate = doc[safeBase][safeTarget].as<float>();
            data.updated = true;

            Serial.printf("CurrencyService: Success! %s -> %s = %.6f (Date: %s)\n",
                          data.base.c_str(), data.target.c_str(), data.rate, data.date.c_str());
            return true;
        }
        if (i == 0) Serial.println("CurrencyService: Primary API failed. Switching to fallback...");
    }

    Serial.println("CurrencyService: All endpoints failed.");
    return false;
}