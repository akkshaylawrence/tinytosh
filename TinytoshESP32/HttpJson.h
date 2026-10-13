#ifndef HTTP_JSON_H
#define HTTP_JSON_H

#include <ArduinoJson.h>

struct HttpJsonOptions {
    uint32_t timeoutMs = 5000;
    const JsonDocument* filter = nullptr;
    const char* userAgent = nullptr;
    const char* referer = nullptr;
    const String* postBody = nullptr;   // sent as JSON; the request is a GET without one
    // Parse the body as it arrives instead of reading it into a String first. Saves holding a
    // large body twice, but only works with servers that do not send it in chunks.
    bool stream = false;
};

// Requests url and parses the reply into a document of the given capacity. The document is null,
// with the reason logged under tag, unless the status is 200 and the body is JSON.
//
// The document is allocated only once the reply has started. A TLS handshake needs nearly all
// the free heap this device has, so memory taken before it makes the connection fail.
DynamicJsonDocument fetchJson(const char* tag, const String& url, size_t capacity, const HttpJsonOptions& options = {});

#endif
