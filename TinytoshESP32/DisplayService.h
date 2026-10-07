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
    
    void showStartup(const Config& config, const String& headline, const String& detail, int percent, bool ok = true);
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

private:
    uint8_t screenBufferOld[1024];
    uint8_t screenBufferNew[1024];

    int currentScreen = 0;
    int currentSubScreen = 0;

    static const int CONTRAST_DIM = 1;
    static const int CONTRAST_MAX = 255;

    static const unsigned long STATIC_REFRESH_MS = 1000;
    static const unsigned long ANIMATED_REFRESH_MS = 40;
    int getFirstEnabledScreen(const AppState& state);

    int getNextAnimationEffect(uint16_t mask);
    void animateHorizontal(int prev, int pSub, int next, int nSub, const AppState& state);
    void animateVertical(int prev, int pSub, int next, int nSub, const AppState& state);
    void animateDissolve(int prev, int pSub, int next, int nSub, const AppState& state);
    void animateCurtain(int prev, int pSub, int next, int nSub, const AppState& state);
    void animateBlinds(int prev, int pSub, int next, int nSub, const AppState& state);
};

#endif