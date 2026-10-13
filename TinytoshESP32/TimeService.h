#ifndef TIME_SERVICE_H
#define TIME_SERVICE_H

#include <Arduino.h>
#include <time.h>

#include "structs.h"

class TimeService {
public:
    TimeService();
    void syncNTP(const String& ianaTimezone, const String& ntpServer);
    bool fetchLocationData(Config& config);
    // Fills out with the local time and says whether the clock has been set. Unlike the core's
    // getLocalTime it never waits, so it is safe to call from loop() before the first sync.
    static bool localTime(struct tm& out);
    static String getCurrentTimeShort(String format);
    static String getCurrentTime(String format);
    static String getFullDate();
    static String formatMinsFromMidnight(int mins, String format, bool show_ampm = true);
    static String formatDurationMins(int mins);
    static int parseTimeToMinsFromMidnight(String apiTime, String format = "12");
    static int parseDurationToMins(String apiDuration);
    static String lookupPosixTimezone(const String& ianaTimezone);
    static int getActiveNightAction(const Config& config);

private:
    static constexpr const char* LOCATION_API_URL = "http://ip-api.com/json/";
    static constexpr const char* DEFAULT_NTP_SERVER = "pool.ntp.org";
    static const time_t SYNCED_AFTER_EPOCH = 1672531200;   // 2023-01-01, later than any unset clock
    // SNTP keeps the pointer it is given, so the name must outlive the config String.
    static char customNtpServer[64];
    const long  gmtOffset_sec = 0;
    const int   daylightOffset_sec = 0;

    static bool isTimeInWindow(int currentMins, const String& startStr, const String& endStr);
};

#endif