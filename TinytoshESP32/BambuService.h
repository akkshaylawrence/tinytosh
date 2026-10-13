#ifndef BAMBU_SERVICE_H
#define BAMBU_SERVICE_H

#include <atomic>

#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include "structs.h"

class BambuService {
public:
    BambuService();
    void begin(Config* config, BambuData* data);
    void loop();

private:
    static const int BAMBU_MQTT_PORT = 8883;
    static const int BAMBU_BUFFER_SIZE = 8192;
    static const int BAMBU_TLS_TIMEOUT_SEC = 5;
    static const unsigned long BAMBU_RECONNECT_INTERVAL_MS = 30000;
    static const unsigned long BAMBU_DATA_TIMEOUT_MS = 60000;
    static const unsigned long BAMBU_SCAN_COOLDOWN_MS = 3000;
    static const int BAMBU_PROBE_KNOWN_IP_MS = 1000;
    static const int BAMBU_PROBE_SCAN_MS = 250;
    static const int BAMBU_JSON_DOC_SIZE = 12288;
    static const int BAMBU_SCAN_YIELD_MS = 10;

    WiFiClientSecure espClient;
    PubSubClient client;
    Config* config;
    BambuData* data;
    unsigned long lastReconnectAttempt;
    unsigned long lastMessageTime;
    
    // The scanner task reads scanBaseIp and writes scanFoundIp, then raises scanFound. loop()
    // touches neither while isScanning is set, and is the only one to change the config.
    std::atomic<bool> isScanning{false};
    std::atomic<bool> scanFound{false};
    String scanBaseIp;
    String scanFoundIp;

    static BambuService* instance;
    static void mqttCallbackStatic(char* topic, byte* payload, unsigned int length);
    static void scanNetworkTask(void* parameter);
    
    void mqttCallback(char* topic, byte* payload, unsigned int length);
    void startScannerSafely();
};

#endif