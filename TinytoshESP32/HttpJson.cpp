#include "HttpJson.h"

#include <HTTPClient.h>

template <typename Source>
static DeserializationError parse(JsonDocument& doc, Source& source, const JsonDocument* filter) {
    return filter ? deserializeJson(doc, source, DeserializationOption::Filter(*filter)) : deserializeJson(doc, source);
}

// A TLS session holds two 16 KB buffers and its handshake state. Measured on an ESP32-C3: a
// handshake started with 48 KB free ran the heap down to 16 bytes and then failed, slowly, and
// with nothing left for WiFi or the web server in the meantime. Refusing up front costs nothing.
static const uint32_t TLS_MIN_FREE_HEAP = 56 * 1024;

DynamicJsonDocument fetchJson(const char* tag, const String& url, size_t capacity, const HttpJsonOptions& options) {
    if (url.startsWith("https") && ESP.getFreeHeap() < TLS_MIN_FREE_HEAP) {
        Serial.printf("%s: Skipped, %u bytes free is too little for HTTPS.\n", tag, (unsigned)ESP.getFreeHeap());
        return DynamicJsonDocument(0);
    }

    HTTPClient http;
    http.setReuse(false);
    http.begin(url);
    if (options.userAgent) http.setUserAgent(options.userAgent);
    http.setConnectTimeout(options.timeoutMs);
    http.setTimeout(options.timeoutMs);

    int httpCode;
    if (options.postBody) {
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Accept", "application/json");
        if (options.referer) http.addHeader("Referer", options.referer);
        httpCode = http.POST(*options.postBody);
    } else {
        httpCode = http.GET();
    }
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("%s: HTTP request failed, code: %d (free heap %u, largest block %u)\n", tag, httpCode,
                      (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
        http.end();
        return DynamicJsonDocument(0);
    }

    String payload;
    if (!options.stream) payload = http.getString();
    DynamicJsonDocument doc(capacity);
    DeserializationError error = options.stream ? parse(doc, http.getStream(), options.filter) : parse(doc, payload, options.filter);
    http.end();
    if (error) {
        Serial.printf("%s: JSON parsing failed: %s\n", tag, error.c_str());
        return DynamicJsonDocument(0);
    }
    return doc;
}
