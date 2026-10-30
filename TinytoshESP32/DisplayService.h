#ifndef DISPLAY_SERVICE_H
#define DISPLAY_SERVICE_H

#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "structs.h"

class DisplayService {
public:
    Adafruit_SSD1306 display;

    DisplayService(int width, int height, int reset_pin);
    void begin(int sda, int scl);
    
    void showStartup(const Config& config, const String& headline, int percent);
    void showStartupFailed(const Config& config, const String& headline, const String& detail);
    void showNotice(const Config& config, const String& title, std::initializer_list<String> lines);
    void showAlert(const Config& config, const unsigned char* icon, const String& text);

    void drawScreen(int screenIndex, const AppState& state, int subIndex = 0);
    void animateTransition(int prevScreen, int prevSub, int nextScreen, int nextSub, const AppState& state);

    bool isScreenEnabled(const AppState& state, int screenIndex);
    void drawCurrentScreen(const AppState& state);
    void switchToNextScreen(const AppState& state);
    void switchToPreviousScreen(const AppState& state);
    void jumpToFirstEnabledScreen(const AppState& state);
    bool isOnFirstEnabledScreen(const AppState& state);

    void setContrast(bool dim);
    unsigned long refreshIntervalMs(const AppState& state) const;
    // Works out the mouse badge. True when the one on the panel is out of date, so a gesture
    // shows at once. Call every loop.
    bool badgeChanged(const AppState& state);

private:
    uint8_t screenBufferOld[1024];
    uint8_t screenBufferNew[1024];

    int currentScreen = 0;
    int currentSubScreen = 0;
    uint8_t badge = 0;
    uint8_t drawnBadge = 0;
    unsigned long movingUntil = 0;
    uint16_t seenGestures = 0;

    static const int CONTRAST_DIM = 1;
    static const int CONTRAST_MAX = 255;

    static const unsigned long STATIC_REFRESH_MS = 1000;
    static const unsigned long ANIMATED_REFRESH_MS = 40;
    static const unsigned long BADGE_MOVING_REFRESH_MS = 80;
    static const unsigned long BADGE_BLINK_REFRESH_MS = 500;
    static const unsigned long MOVING_HOLD_MS = 2500;
    int getFirstEnabledScreen(const AppState& state);

    int getNextAnimationEffect(uint16_t mask);
    void animateHorizontal(int prev, int pSub, int next, int nSub, const AppState& state);
    void animateVertical(int prev, int pSub, int next, int nSub, const AppState& state);
    void animateDissolve(int prev, int pSub, int next, int nSub, const AppState& state);
    void animateCurtain(int prev, int pSub, int next, int nSub, const AppState& state);
    void animateBlinds(int prev, int pSub, int next, int nSub, const AppState& state);
};

#endif