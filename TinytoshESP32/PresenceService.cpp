#include "PresenceService.h"

#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <esp_random.h>

namespace {

const uint8_t REPORT_ID = 1;

// Three buttons, then relative X, Y and wheel as signed bytes.
const uint8_t MOUSE_REPORT_MAP[] = {
  0x05, 0x01,       // Usage Page (Generic Desktop)
  0x09, 0x02,       // Usage (Mouse)
  0xA1, 0x01,       // Collection (Application)
  0x85, REPORT_ID,  //   Report ID
  0x09, 0x01,       //   Usage (Pointer)
  0xA1, 0x00,       //   Collection (Physical)
  0x05, 0x09,       //     Usage Page (Button)
  0x19, 0x01,       //     Usage Minimum (1)
  0x29, 0x03,       //     Usage Maximum (3)
  0x15, 0x00,       //     Logical Minimum (0)
  0x25, 0x01,       //     Logical Maximum (1)
  0x95, 0x03,       //     Report Count (3)
  0x75, 0x01,       //     Report Size (1)
  0x81, 0x02,       //     Input (Data, Variable, Absolute)
  0x95, 0x01,       //     Report Count (1)
  0x75, 0x05,       //     Report Size (5)
  0x81, 0x03,       //     Input (Constant)
  0x05, 0x01,       //     Usage Page (Generic Desktop)
  0x09, 0x30,       //     Usage (X)
  0x09, 0x31,       //     Usage (Y)
  0x09, 0x38,       //     Usage (Wheel)
  0x15, 0x81,       //     Logical Minimum (-127)
  0x25, 0x7F,       //     Logical Maximum (127)
  0x75, 0x08,       //     Report Size (8)
  0x95, 0x03,       //     Report Count (3)
  0x81, 0x06,       //     Input (Data, Variable, Relative)
  0xC0,             //   End Collection
  0xC0              // End Collection
};

}  // namespace

void PresenceService::begin(const char* deviceName) {
    BLEDevice::init(deviceName);

    // Bonding without a passkey, as a mouse has neither a display nor keys to confirm one.
    BLESecurity* security = new BLESecurity();
    security->setCapability(ESP_IO_CAP_NONE);
    security->setAuthenticationMode(true, false, true);

    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(this);

    BLEHIDDevice* hid = new BLEHIDDevice(server);
    hid->manufacturer()->setValue("Tinytosh");
    hid->pnp(0x02, 0x303A, 0x8001, 0x0100);
    hid->hidInfo(0x00, 0x01);
    hid->reportMap((uint8_t*)MOUSE_REPORT_MAP, sizeof(MOUSE_REPORT_MAP));
    input = hid->inputReport(REPORT_ID);
    hid->setBatteryLevel(100);
    hid->startServices();

    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->setAppearance(HID_MOUSE);
    advertising->addServiceUUID(hid->hidService()->getUUID());
    advertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    latest.link = Presence::LINK_ADVERTISING;
    started = true;
    xTaskCreate(taskEntry, "presence", TASK_STACK_BYTES, this, 1, nullptr);
    Serial.printf("PresenceService: Bluetooth mouse \"%s\" advertising. Free heap: %u\n", deviceName, ESP.getFreeHeap());
}

void PresenceService::submit(const Presence::Command& command) {
    portENTER_CRITICAL(&mailbox);
    pending = command;
    hasPending = true;
    portEXIT_CRITICAL(&mailbox);
}

Presence::Status PresenceService::status() {
    portENTER_CRITICAL(&mailbox);
    Presence::Status copy = latest;
    portEXIT_CRITICAL(&mailbox);
    return copy;
}

void PresenceService::taskEntry(void* self) {
    static_cast<PresenceService*>(self)->run();
}

void PresenceService::run() {
    for (;;) {
        Presence::Command command;
        portENTER_CRITICAL(&mailbox);
        const bool fresh = hasPending;
        if (fresh) command = pending;
        hasPending = false;
        portEXIT_CRITICAL(&mailbox);

        if (fresh) engine.apply(command, millis());
        const bool linkUp = connected;
        const uint32_t waitMs = engine.step(millis(), linkUp);

        Presence::Status now = engine.status(millis());
        now.link = linkUp ? Presence::LINK_CONNECTED : Presence::LINK_ADVERTISING;
        portENTER_CRITICAL(&mailbox);
        latest = now;
        portEXIT_CRITICAL(&mailbox);

        vTaskDelay(pdMS_TO_TICKS(waitMs ? waitMs : 1));
    }
}

uint16_t PresenceService::random16() {
    return esp_random();
}

bool PresenceService::emit(int8_t dx, int8_t dy, int8_t scroll) {
    if (!connected) return false;
    uint8_t report[4] = {0, (uint8_t)dx, (uint8_t)dy, (uint8_t)scroll};
    input->setValue(report, sizeof(report));
    input->notify();
    return true;
}

void PresenceService::onConnect(BLEServer*) {
    connected = true;
    Serial.println("PresenceService: Host connected.");
}

void PresenceService::onDisconnect(BLEServer*) {
    connected = false;
    Serial.println("PresenceService: Host disconnected. Advertising again.");
    BLEDevice::startAdvertising();
}
