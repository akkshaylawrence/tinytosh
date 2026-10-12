#ifndef PRESENCE_SERVICE_H
#define PRESENCE_SERVICE_H

// The Bluetooth mouse and the task that runs the gesture engine on it.
//
// The engine runs on its own task because loop() can block for seconds on a data sync or a
// screen transition, which would freeze a stroke half way. That task is the only code that
// touches the engine. loop() leaves it the host's latest command and picks up a status copy.

#include <Arduino.h>
#include <BLEServer.h>

#include "PresenceEngine.h"

class BLECharacteristic;

class PresenceService : private Presence::Port, private BLEServerCallbacks {
public:
    void begin(const char* deviceName);
    bool running() const { return started; }

    void submit(const Presence::Command& command);
    Presence::Status status();

private:
    static const uint32_t TASK_STACK_BYTES = 4096;

    Presence::Engine engine{*this};
    BLECharacteristic* input = nullptr;
    volatile bool connected = false;
    bool started = false;

    portMUX_TYPE mailbox = portMUX_INITIALIZER_UNLOCKED;
    Presence::Command pending;
    bool hasPending = false;
    Presence::Status latest;

    static void taskEntry(void* self);
    void run();

    uint16_t random16() override;
    bool emit(int8_t dx, int8_t dy, int8_t scroll) override;

    void onConnect(BLEServer* server) override;
    void onDisconnect(BLEServer* server) override;
};

#endif
