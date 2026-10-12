#ifndef JSON_SERIALIZER_H
#define JSON_SERIALIZER_H

#include <ArduinoJson.h>
#include "structs.h"

class JsonSerializer {
public:
    static String buildAppStateJson(const AppState& state);
    static String buildConfigJson(const Config& config);
    static bool parseConfig(const char* jsonString, AppState& state);

    // The "presence" object of a stats message. Leaves the state alone when there is none.
    static bool parsePresence(JsonVariantConst stats, AppState& state);
    static String buildPresenceJson(const Presence::Status& status);

private:
    static void populateConfigDoc(const Config& config, JsonObject configObj);
};

#endif