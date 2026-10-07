// The simulator's side of ScreenHost: local time from the seeded clock, and no network.

#include "ScreenHost.h"

namespace ScreenHost {

struct tm localNow() {
    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);
    return local;
}

String clock(const String& format) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char time_str[12];
    if (format == "12") strftime(time_str, sizeof(time_str), "%I:%M", &timeinfo);
    else strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
    return String(time_str);
}

String fullDate() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "No Date";
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%A, %b %d", &timeinfo);
    return String(buffer);
}

String formatMinsFromMidnight(int mins, const String& format, bool show_ampm) {
    if (mins == -1) return "--:--";
    int h = mins / 60;
    int m = mins % 60;
    char buf[12];
    if (format == "24") {
        snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    } else {
        int displayH = h % 12;
        if (displayH == 0) displayH = 12;
        if (show_ampm) {
            const char* ampm = (h >= 12) ? "PM" : "AM";
            snprintf(buf, sizeof(buf), "%02d:%02d %s", displayH, m, ampm);
        } else {
            snprintf(buf, sizeof(buf), "%02d:%02d", displayH, m);
        }
    }
    return String(buf);
}

String formatDurationMins(int mins) {
    if (mins == -1) return "--";
    char buf[12];
    snprintf(buf, sizeof(buf), "%dh %dm", mins / 60, mins % 60);
    return String(buf);
}

String weatherDescription(int wmo_code) {
    if (wmo_code == 0) return "Clear Sky";
    if (wmo_code >= 1 && wmo_code <= 3) return "Cloudy";
    if (wmo_code >= 45 && wmo_code <= 48) return "Fog";
    if (wmo_code >= 51 && wmo_code <= 67) return "Rain";
    if (wmo_code >= 71 && wmo_code <= 77) return "Snow";
    if (wmo_code >= 95) return "Thunder";
    return "Unknown";
}

long long livePopulation(long long basePop, double growth, int year) {
    if (basePop <= 0 || year <= 0) return basePop;

    struct tm timeinfo = {0};
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = 0;
    timeinfo.tm_mday = 1;

    time_t baseline = mktime(&timeinfo);
    time_t now = time(nullptr);

    if (now < baseline) return basePop;

    double growth_per_sec = (basePop * (growth / 100.0)) / 31557600.0;
    double diff_sec = difftime(now, baseline);

    return basePop + (long long)(diff_sec * growth_per_sec);
}

}  // namespace ScreenHost
