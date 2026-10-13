#ifndef DATA_SYNC_SERVICE_H
#define DATA_SYNC_SERVICE_H

#include <atomic>

#include "structs.h"

// A background sync never touches AppState. loop() hands the task a copy of what it needs in
// a SyncJob, the task fetches into that copy, and loop() copies the fetched parts back once the
// task says it is done. So the display and the web server only ever see writes from loop().
struct SyncJob {
    Config config;
    bool nightModeLatched = false;
    bool dueWeather = false;
    bool dueAqi = false;
    bool dueFlight = false;
    bool dueCurrency = false;

    WeatherData weather;
    AirQualityData aqi;
    DaylightData daylight;
    MoonData moon;
    PopulationData population;
    FlightData flight;
    CurrencyData currencies[MAX_MULTI_ENTRIES];

    bool offline = false;
    bool fetchedDaylight = false;
    bool fetchedMoon = false;
    bool fetchedPopulation = false;
};

class DataSyncService {
public:
    bool isGlobalSyncDue(const Config& config, bool nightModeLatched) const;
    void markGlobalSynced();
    bool isDue(ScreenType screen, const Config& config, bool nightModeLatched) const;
    void markFetched(ScreenType screen);
    void runFullSync(AppState& state);
    void maybeStartBackgroundSync(AppState& state, bool nightModeLatched);
    // Call from loop(). Moves a finished background sync's data into the state.
    void applyFinishedBackgroundSync(AppState& state);

private:
    static const int NIGHT_INTERVAL_MULTIPLIER = 10;
    static const unsigned long RETRY_BACKOFF_MS = 30000;

    FetchTrackers trackers;

    // The task owns job from the moment it is created until it sets jobFinished.
    SyncJob job;
    std::atomic<bool> jobRunning{false};
    std::atomic<bool> jobFinished{false};
    unsigned long lastFailedAttempt = 0;

    unsigned long getGlobalIntervalMs(const Config& config, bool nightModeLatched) const;
    bool isFetchDue(unsigned long lastFetch, int customMin, const Config& config, bool nightModeLatched) const;

    static void backgroundSyncTaskTrampoline(void* parameter);
    void runBackgroundSyncBody();
};

#endif
