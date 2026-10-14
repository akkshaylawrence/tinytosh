#include "FlightService.h"

#include <ArduinoJson.h>
#include <math.h>

#include "HttpJson.h"

bool FlightService::fetchFlights(const Config& config, FlightData& data) {
    if (config.flight_mode == "closest") {
        FlightAircraft nearest[1];
        int count = 0;
        int totalCount = 0;
        if (!fetchNearbyAircraft(config, nearest, 1, count, totalCount)) return false;

        if (count == 0) {
            Serial.println("FlightService: No airborne aircraft found nearby.");
            data.closest = FlightAircraft();
            data.aircraft_count = totalCount;
            return true;
        }

        String originCity, destCity;
        FlightAircraft closest = nearest[0];
        fetchRoutes(&closest, 1, &originCity, &destCity);

        bool isNewAircraft = (data.closest.icao24 != closest.icao24);
        String manufacturer = isNewAircraft ? "" : data.aircraft_manufacturer;
        String type = isNewAircraft ? "" : data.aircraft_type;
        fetchAircraftDetails(closest.icao24, manufacturer, type);

        data.closest = closest;
        data.aircraft_count = totalCount;
        data.origin_city = originCity;
        data.destination_city = destCity;
        data.aircraft_manufacturer = manufacturer;
        data.aircraft_type = type;

        return true;
    }

    FlightAircraft buffer[MAX_RADAR_AIRCRAFT];
    int count = 0;
    int totalCount = 0;
    if (!fetchNearbyAircraft(config, buffer, MAX_RADAR_AIRCRAFT, count, totalCount)) return false;

    if ((config.flight_primary_info == "route" || config.flight_secondary_info == "route") && count > 0) {
        fetchRoutes(buffer, count, nullptr, nullptr);
    }

    for (int i = 0; i < count; i++) data.aircraft[i] = buffer[i];
    data.aircraft_count = count;

    return true;
}

bool FlightService::fetchNearbyAircraft(const Config& config, FlightAircraft* out, int maxOut, int& outCount, int& outTotalCount) {
    outCount = 0;
    outTotalCount = 0;

    String url = String(ADSBLOL_BASE_URL) + "/lat/" + String(config.latitude, 4) +
                 "/lon/" + String(config.longitude, 4) + "/dist/" + String(config.flight_radius_nm);

    Serial.println("FlightService: Fetching nearby aircraft from ADSB.lol (radius " + String(config.flight_radius_nm) + "nm) -> " + url);

    DynamicJsonDocument filter(256);
    filter["hex"] = true;
    filter["flight"] = true;
    filter["lat"] = true;
    filter["lon"] = true;
    filter["alt_baro"] = true;
    filter["gs"] = true;
    filter["track"] = true;
    filter["r"] = true;
    filter["t"] = true;
    filter["squawk"] = true;

    auto consider = [&](JsonObjectConst state) {
        if (state["lat"].isNull() || state["lon"].isNull()) return;

        float altitude = parseAltitudeFt(state["alt_baro"]);
        if (!isnan(altitude) && altitude <= 0) return;

        String callsign = state["flight"].isNull() ? "" : state["flight"].as<String>();
        callsign.trim();
        if (callsign.length() == 0) return;

        outTotalCount++;

        float lat = state["lat"].as<float>();
        float lon = state["lon"].as<float>();
        float distance = haversineKm(config.latitude, config.longitude, lat, lon);

        int insertAt = outCount;
        while (insertAt > 0 && out[insertAt - 1].distance_km > distance) insertAt--;
        if (insertAt >= maxOut) return;

        int last = (outCount < maxOut) ? outCount : maxOut - 1;
        for (int i = last; i > insertAt; i--) out[i] = out[i - 1];

        FlightAircraft& slot = out[insertAt];
        slot.callsign = callsign;
        slot.icao24 = state["hex"].isNull() ? "" : state["hex"].as<String>();
        slot.lat = lat;
        slot.lon = lon;
        slot.altitude_ft = altitude;
        slot.velocity_kt = state["gs"].isNull() ? NAN : state["gs"].as<float>();
        slot.track_deg = state["track"].isNull() ? NAN : state["track"].as<float>();
        slot.distance_km = distance;
        slot.registration = state["r"].isNull() ? "" : state["r"].as<String>();
        slot.type_designator = state["t"].isNull() ? "" : state["t"].as<String>();
        slot.squawk = state["squawk"].isNull() ? "" : state["squawk"].as<String>();
        slot.origin_code = "";
        slot.destination_code = "";
        slot.origin_country = "";
        slot.destination_country = "";
        slot.has_route = false;

        if (outCount < maxOut) outCount++;
    };

    // A busy sky is tens of kilobytes of JSON, more than the heap has beside an open TLS session.
    // So the "ac" array is read one aircraft at a time, and only the nearest few are kept.
    HttpJsonOptions options;
    options.timeoutMs = HTTP_TIMEOUT_MS;
    options.userAgent = USER_AGENT;
    bool ok = fetchStream("FlightService", url, options, [&](Stream& body) {
        if (!body.find("\"ac\":[")) {
            Serial.println("FlightService: No aircraft in range.");
            return true;
        }
        while (isspace(body.peek())) body.read();
        if (body.peek() == ']') return true;

        DynamicJsonDocument state(512);
        do {
            DeserializationError error = deserializeJson(state, body, DeserializationOption::Filter(filter));
            if (error) {
                Serial.printf("FlightService: JSON parsing failed: %s\n", error.c_str());
                return false;
            }
            consider(state.as<JsonObjectConst>());
        } while (body.findUntil(",", "]"));
        return true;
    });
    if (!ok) return false;

    Serial.printf("FlightService: Success! %d aircraft kept after distance ranking.\n", outCount);
    return true;
}

void FlightService::fetchRoutes(FlightAircraft* aircraft, int count, String* outOriginCity, String* outDestCity) {
    DynamicJsonDocument reqDoc(1536);
    JsonArray planes = reqDoc.createNestedArray("planes");

    for (int i = 0; i < count; i++) {
        if (aircraft[i].callsign.length() == 0) continue;
        JsonObject p = planes.createNestedObject();
        p["callsign"] = aircraft[i].callsign;
        p["lat"] = aircraft[i].lat;
        p["lng"] = aircraft[i].lon;
    }

    if (planes.size() == 0) return;

    String payload;
    serializeJson(reqDoc, payload);

    Serial.println("FlightService: Looking up routes for " + String(planes.size()) + " aircraft -> " + String(ADSBLOL_ROUTE_URL) + " (" + payload + ")");

    DynamicJsonDocument filter(256);
    JsonObject routeFilter = filter.createNestedObject();
    routeFilter["callsign"] = true;
    routeFilter["airline_code"] = true;
    JsonObject airportFilter = routeFilter.createNestedArray("_airports").createNestedObject();
    airportFilter["iata"] = true;
    airportFilter["icao"] = true;
    airportFilter["countryiso2"] = true;
    airportFilter["location"] = true;

    HttpJsonOptions options;
    options.timeoutMs = HTTP_TIMEOUT_MS;
    options.stream = true;
    options.userAgent = USER_AGENT;
    options.referer = HEADER_REFERER;
    options.postBody = &payload;
    options.filter = &filter;
    // Filtered, a route with a few airports takes under 512 bytes. Sizing the document by the
    // planes asked about keeps it small enough to allocate beside an open TLS session.
    DynamicJsonDocument doc = fetchJson("FlightService", ADSBLOL_ROUTE_URL, 768 * planes.size(), options);
    if (doc.isNull()) return;

    JsonArray routes = doc.as<JsonArray>();
    if (routes.isNull()) return;

    Serial.printf("FlightService: Received %d route record(s).\n", routes.size());

    for (JsonObject route : routes) {
        String callsign = route["callsign"].isNull() ? "" : route["callsign"].as<String>();
        callsign.trim();
        callsign.toUpperCase();
        if (callsign.length() == 0) continue;

        JsonArray airports = route["_airports"].as<JsonArray>();
        if (airports.isNull() || airports.size() == 0) continue;

        JsonObject origin = airports[0];
        JsonObject destination = airports[airports.size() - 1];

        String originCode = !origin["iata"].isNull() ? origin["iata"].as<String>() : (!origin["icao"].isNull() ? origin["icao"].as<String>() : "");
        String destCode = !destination["iata"].isNull() ? destination["iata"].as<String>() : (!destination["icao"].isNull() ? destination["icao"].as<String>() : "");
        String originCountry = origin["countryiso2"].isNull() ? "" : origin["countryiso2"].as<String>();
        String destCountry = destination["countryiso2"].isNull() ? "" : destination["countryiso2"].as<String>();
        String airlineCode = route["airline_code"].isNull() ? "" : route["airline_code"].as<String>();

        for (int i = 0; i < count; i++) {
            String candidate = aircraft[i].callsign;
            candidate.toUpperCase();
            if (candidate != callsign) continue;

            aircraft[i].origin_code = originCode;
            aircraft[i].destination_code = destCode;
            aircraft[i].origin_country = originCountry;
            aircraft[i].destination_country = destCountry;
            aircraft[i].airline_code = airlineCode;
            aircraft[i].has_route = (originCode.length() > 0 && destCode.length() > 0);

            if (i == 0 && outOriginCity && outDestCity) {
                *outOriginCity = origin["location"].isNull() ? "" : origin["location"].as<String>();
                *outDestCity = destination["location"].isNull() ? "" : destination["location"].as<String>();
            }
            break;
        }
    }

    int matched = 0;
    for (int i = 0; i < count; i++) {
        if (!aircraft[i].has_route) continue;
        matched++;
        Serial.printf("FlightService:   %s: %s -> %s (%s)\n", aircraft[i].callsign.c_str(), aircraft[i].origin_code.c_str(), aircraft[i].destination_code.c_str(), aircraft[i].airline_code.c_str());
    }
    Serial.printf("FlightService: Success! %d/%d aircraft matched a route.\n", matched, count);
}

void FlightService::fetchAircraftDetails(const String& icao24, String& outManufacturer, String& outType) {
    if (icao24.length() == 0) return;

    String url = String(ADSBDB_AIRCRAFT_URL) + icao24;
    Serial.println("FlightService: Fetching aircraft metadata from ADSBDB -> " + url);

    HttpJsonOptions options;
    options.timeoutMs = HTTP_TIMEOUT_MS;
    options.stream = true;
    DynamicJsonDocument doc = fetchJson("FlightService", url, 1024, options);
    if (doc.isNull()) return;

    JsonObject aircraft = doc["response"]["aircraft"];
    if (aircraft.isNull()) {
        Serial.println("FlightService: ADSBDB response had no aircraft info.");
        return;
    }

    outManufacturer = aircraft["manufacturer"].isNull() ? "" : aircraft["manufacturer"].as<String>();
    outType = aircraft["type"].isNull() ? "" : aircraft["type"].as<String>();

    Serial.println("FlightService: Success! Manufacturer: " + outManufacturer + " Type: " + outType);
}

float FlightService::haversineKm(float lat1, float lon1, float lat2, float lon2) {
    float lat1Rad = radians(lat1);
    float lat2Rad = radians(lat2);
    float dLat = radians(lat2 - lat1);
    float dLon = radians(lon2 - lon1);

    float a = sin(dLat / 2) * sin(dLat / 2) + cos(lat1Rad) * cos(lat2Rad) * sin(dLon / 2) * sin(dLon / 2);
    float c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return EARTH_RADIUS_KM * c;
}

float FlightService::parseAltitudeFt(JsonVariantConst v) {
    if (v.isNull()) return NAN;
    if (v.is<float>() || v.is<int>()) return v.as<float>();
    if (v.is<const char*>()) {
        String s = v.as<String>();
        if (s.equalsIgnoreCase("ground")) return 0.0;
        return s.toFloat();
    }
    return NAN;
}
