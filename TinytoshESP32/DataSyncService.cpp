#include "DataSyncService.h"

#include <Arduino.h>
#include <time.h>
#include <WiFi.h>

#include "AirQualityService.h"
#include "ConfigManager.h"
#include "CurrencyService.h"
#include "DaylightService.h"
#include "DisplayService.h"
#include "FlightService.h"
#include "MoonService.h"
#include "PopulationService.h"
#include "TimeService.h"
#include "WeatherService.h"


extern ConfigManager configManager;
extern DisplayService displayService;
extern TimeService timeService;
extern WeatherService weatherService;
extern AirQualityService airQualityService;
extern DaylightService daylightService;
extern MoonService moonService;
extern PopulationService populationService;
extern CurrencyService currencyService;
extern FlightService flightService;

unsigned long DataSyncService::intervalMs(unsigned long minutes, bool nightModeLatched) {
    return minutes * 60000UL * (nightModeLatched ? NIGHT_INTERVAL_MULTIPLIER : 1);
}

bool DataSyncService::isFetchDue(unsigned long lastFetch, int customMin, const Config& config, bool nightModeLatched) const {
    if (customMin <= 0) {
        return isGlobalSyncDue(config, nightModeLatched);
    }

    return (millis() - lastFetch) > intervalMs(customMin, nightModeLatched);
}

bool DataSyncService::isGlobalSyncDue(const Config& config, bool nightModeLatched) const {
    return (millis() - trackers.lastDataUpdate) > intervalMs(config.refresh_interval_min, nightModeLatched);
}

void DataSyncService::markGlobalSynced() {
    trackers.lastDataUpdate = millis();
}

bool DataSyncService::isDue(ScreenType screen, const Config& config, bool nightModeLatched) const {
    switch (screen) {
        case SCREEN_WEATHER:
            return config.show_weather && isFetchDue(trackers.lastWeatherFetch, config.custom_weather_int_min, config, nightModeLatched);
        case SCREEN_AIR_QUALITY:
            return config.show_aqi && isFetchDue(trackers.lastAqiFetch, config.custom_aqi_int_min, config, nightModeLatched);
        case SCREEN_FLIGHT:
            return config.show_flight && isFetchDue(trackers.lastFlightFetch, config.custom_flight_int_min, config, nightModeLatched);
        case SCREEN_CURRENCY:
            return config.show_currency && isFetchDue(trackers.lastCurrencyFetch, config.custom_currency_int_min, config, nightModeLatched);
        default:
            return false;
    }
}

void DataSyncService::markFetched(ScreenType screen) {
    unsigned long now = millis();
    switch (screen) {
        case SCREEN_WEATHER: trackers.lastWeatherFetch = now; break;
        case SCREEN_AIR_QUALITY: trackers.lastAqiFetch = now; break;
        case SCREEN_FLIGHT: trackers.lastFlightFetch = now; break;
        case SCREEN_CURRENCY: trackers.lastCurrencyFetch = now; break;
        default: break;
    }
}

namespace {

// The boot card's headline and how far along the bar sits for each step of a full sync.
struct SyncStep {
    const char* headline;
    int percent;
};

const SyncStep STEP_LOCATION    = {"Finding Location", 20};
const SyncStep STEP_TIME        = {"Setting the Clock", 28};
const SyncStep STEP_WEATHER     = {"Updating Weather", 36};
const SyncStep STEP_AIR_QUALITY = {"Updating Air Quality", 44};
const SyncStep STEP_DAYLIGHT    = {"Updating Daylight", 52};
const SyncStep STEP_MOON        = {"Updating Moon", 60};
const SyncStep STEP_POPULATION  = {"Updating Population", 68};
const SyncStep STEP_FLIGHTS     = {"Updating Flights", 78};
const SyncStep STEP_CURRENCY    = {"Updating Currency", 88};
const SyncStep STEP_READY       = {"Tinytosh is Ready", 100};

}  // namespace

void DataSyncService::runFullSync(AppState& state) {
    // A background sync still in flight was planned from the settings as they were. Let it end
    // and drop what it fetched, so it cannot put back data this sync is about to replace.
    while (jobRunning && !jobFinished) delay(50);
    jobFinished = false;
    jobRunning = false;

    Config& config = state.config;
    auto announce = [&](const SyncStep& step, const String& detail) {
        displayService.showStartup(config, step.headline, detail, step.percent);
    };

    if (config.auto_detect) {
        announce(STEP_LOCATION, "Please wait");
        if (timeService.fetchLocationData(config)) {
            Serial.println("Location updated via IP");
        }
    }

    // Depends on the timezone the location step may have set.
    announce(STEP_TIME, config.timezone);
    timeService.syncNTP(config.timezone, config.ntp_server);

    struct tm timeinfo;
    TimeService::localTime(timeinfo);
    int current_yday = timeinfo.tm_yday;

    if (config.show_weather) {
        announce(STEP_WEATHER, config.city);
        String updateTime = TimeService::getCurrentTime(config.time_format);
        weatherService.fetchWeather(config, state.weather, updateTime);
        markFetched(SCREEN_WEATHER);
    }

    if (config.show_aqi) {
        announce(STEP_AIR_QUALITY, config.city);
        airQualityService.fetchAirQuality(config, state.aqi);
        markFetched(SCREEN_AIR_QUALITY);
    }

    if (config.show_daylight && state.daylight.last_fetch_yday != current_yday) {
        announce(STEP_DAYLIGHT, config.city);
        daylightService.fetchDaylight(config, state.daylight);
    }

    if (config.show_moon && state.moon.last_fetch_yday != current_yday) {
        announce(STEP_MOON, config.city);
        moonService.fetchMoon(config, state.moon);
    }

    if (config.show_population && state.population.last_fetch_yday != current_yday) {
        String popLoc = (config.pop_show_world && config.pop_show_country && config.country_code != "") ? ("World + " + config.country_code) : (config.pop_show_world ? "World" : config.country_code);
        announce(STEP_POPULATION, popLoc);
        populationService.fetchPopulation(config, state.population);
    }

    if (config.show_flight) {
        announce(STEP_FLIGHTS, "Within " + String(config.flight_radius_nm) + " nm");
        flightService.fetchFlights(config, state.flight);
        markFetched(SCREEN_FLIGHT);
    }

    if (config.show_currency) {
        for (int i = 0; i < config.currency_count; i++) {
            String baseUpper = String(config.currency_bases[i]);
            baseUpper.toUpperCase();
            String targetUpper = String(config.currency_targets[i]);
            targetUpper.toUpperCase();

            announce(STEP_CURRENCY, baseUpper + " to " + targetUpper);
            currencyService.fetchRate(config.currency_bases[i], config.currency_targets[i], state.currencies[i]);
        }
        markFetched(SCREEN_CURRENCY);
    }

    announce(STEP_READY, "Welcome");

    // The location step may have changed the config.
    configManager.saveConfig(config);

    markGlobalSynced();
}

void DataSyncService::maybeStartBackgroundSync(AppState& state, bool nightModeLatched) {
    bool dueGlobal = isGlobalSyncDue(state.config, nightModeLatched);
    bool anyScreenDue = dueGlobal
        || isDue(SCREEN_WEATHER, state.config, nightModeLatched)
        || isDue(SCREEN_AIR_QUALITY, state.config, nightModeLatched)
        || isDue(SCREEN_FLIGHT, state.config, nightModeLatched)
        || isDue(SCREEN_CURRENCY, state.config, nightModeLatched);

    if (!anyScreenDue) return;

    if (lastFailedAttempt != 0 && (millis() - lastFailedAttempt) < RETRY_BACKOFF_MS) return;

    if (jobRunning) return;

    job.config = state.config;
    job.nightModeLatched = nightModeLatched;
    job.dueWeather = isDue(SCREEN_WEATHER, state.config, nightModeLatched);
    job.dueAqi = isDue(SCREEN_AIR_QUALITY, state.config, nightModeLatched);
    job.dueFlight = isDue(SCREEN_FLIGHT, state.config, nightModeLatched);
    job.dueCurrency = isDue(SCREEN_CURRENCY, state.config, nightModeLatched);
    job.weather = state.weather;
    job.aqi = state.aqi;
    job.daylight = state.daylight;
    job.moon = state.moon;
    job.population = state.population;
    job.flight = state.flight;
    for (int i = 0; i < MAX_MULTI_ENTRIES; i++) job.currencies[i] = state.currencies[i];
    job.offline = false;
    job.fetchedDaylight = job.fetchedMoon = job.fetchedPopulation = false;

    jobRunning = true;
    xTaskNotifyGive(worker);
}

void DataSyncService::begin() {
    xTaskCreate(workerEntry, "BgUpdateTask", 8192, this, 1, &worker);
}

void DataSyncService::applyFinishedBackgroundSync(AppState& state) {
    if (!jobFinished) return;

    if (job.offline) {
        lastFailedAttempt = millis();
    } else {
        if (job.fetchedDaylight) state.daylight = job.daylight;
        if (job.fetchedMoon) state.moon = job.moon;
        if (job.fetchedPopulation) state.population = job.population;
        if (job.dueWeather) { state.weather = job.weather; markFetched(SCREEN_WEATHER); }
        if (job.dueAqi) { state.aqi = job.aqi; markFetched(SCREEN_AIR_QUALITY); }
        if (job.dueFlight) { state.flight = job.flight; markFetched(SCREEN_FLIGHT); }
        if (job.dueCurrency) {
            for (int i = 0; i < MAX_MULTI_ENTRIES; i++) state.currencies[i] = job.currencies[i];
            markFetched(SCREEN_CURRENCY);
        }
        if (isGlobalSyncDue(state.config, job.nightModeLatched)) markGlobalSynced();
        lastFailedAttempt = 0;
    }

    jobFinished = false;
    jobRunning = false;
}

void DataSyncService::workerEntry(void* parameter) {
    DataSyncService* service = static_cast<DataSyncService*>(parameter);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        service->runJob();
        service->jobFinished = true;
    }
}

void DataSyncService::runJob() {
    Serial.println("Background Task: Starting API updates...");

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Background Task: WiFi offline. Will retry shortly.");
        job.offline = true;
        return;
    }

    const Config& config = job.config;

    struct tm timeinfo;
    TimeService::localTime(timeinfo);
    int current_yday = timeinfo.tm_yday;

    if (config.show_daylight && job.daylight.last_fetch_yday != current_yday) {
        daylightService.fetchDaylight(config, job.daylight);
        job.fetchedDaylight = true;
    }
    if (config.show_moon && job.moon.last_fetch_yday != current_yday) {
        moonService.fetchMoon(config, job.moon);
        job.fetchedMoon = true;
    }
    if (config.show_population && job.population.last_fetch_yday != current_yday) {
        populationService.fetchPopulation(config, job.population);
        job.fetchedPopulation = true;
    }

    if (job.dueWeather) weatherService.fetchWeather(config, job.weather, TimeService::getCurrentTime(config.time_format));
    if (job.dueAqi) airQualityService.fetchAirQuality(config, job.aqi);
    if (job.dueFlight) flightService.fetchFlights(config, job.flight);
    if (job.dueCurrency) {
        for (int i = 0; i < config.currency_count; i++) currencyService.fetchRate(config.currency_bases[i], config.currency_targets[i], job.currencies[i]);
    }

    Serial.printf("Background Task: Updates complete. Stack headroom: %u\n", (unsigned)uxTaskGetStackHighWaterMark(NULL));
}
