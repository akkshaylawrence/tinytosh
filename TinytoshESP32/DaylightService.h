#ifndef DAYLIGHT_SERVICE_H
#define DAYLIGHT_SERVICE_H

#include <Arduino.h>

#include "structs.h"

class DaylightService {
public:
    DaylightService();
    bool fetchDaylight(const Config& config, DaylightData& data);

private:
    static constexpr const char* DAYLIGHT_API_BASE = "https://api.sunrise-sunset.org/json";
};

#endif