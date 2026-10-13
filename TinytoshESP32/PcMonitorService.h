#ifndef PC_MONITOR_SERVICE_H
#define PC_MONITOR_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "structs.h"

class PcMonitorService {
public:
    bool handleSerial(AppState &state);

    // Takes a stats message from the PC App, however it arrived. Says whether it carried a
    // presence command, which the caller answers with the presence status.
    static bool applyStats(JsonVariantConst stats, AppState &state, bool overWifi);

private:
    static const unsigned long DATA_TIMEOUT_MS = 3000;
    static const unsigned long WIFI_DATA_TIMEOUT_MS = 10000;

    static void parseJson(const char* jsonString, AppState &state);
};

#endif