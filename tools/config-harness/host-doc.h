// ArduinoJson's slots are twice the size on a 64-bit host, so a document sized for the ESP32
// overflows here and silently drops its tail. This doubles every capacity the firmware asks for.
#include <ArduinoJson.h>

struct HostJsonDocument : DynamicJsonDocument {
  explicit HostJsonDocument(size_t capacity) : DynamicJsonDocument(capacity * 2) {}
};
#define DynamicJsonDocument HostJsonDocument
