
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneButton.h>
#include <Wire.h>
#include <time.h>

#include "images.h"
#include "ScreenHost.h"
#include "Screens.h"
#include "structs.h"

Adafruit_SSD1306 display(128, 64, &Wire, -1);
OneButton button;

AppState appState;

int currentScreen = 0;
int currentSubScreen = 0;
unsigned long lastScreenSwitch = 0;

uint8_t screenBufferOld[1024];
uint8_t screenBufferNew[1024];

const int CONTRAST_DIM = 1;
const int CONTRAST_MAX = 255;

const unsigned long ANIMATED_REFRESH_MS = 40;
unsigned long lastAnimatedFrame = 0;

Ui::Surface surfaceFor(const Config& config) {
    return Ui::Surface(display, Ui::styleFrom(config.ui_chrome, config.ui_paper), ScreenHost::clock(config.time_format));
}

void showAlert(const unsigned char* icon, const String& text) {
    display.clearDisplay();
    Ui::Surface ui = surfaceFor(appState.config);
    ui.alert(icon, text);
    ui.finish();
}

bool isScreenEnabled(const AppState& state, int screenIndex) {
    if (screenIndex < 0 || screenIndex >= NUM_SCREENS) return false;
    return SCREENS[screenIndex].enabled(state);
}

int pageCount(const AppState& state, int screenIndex) {
    return SCREENS[screenIndex].pages ? SCREENS[screenIndex].pages(state) : 1;
}

void drawScreen(int screenIndex, const AppState& state, int subIndex = 0) {
    display.clearDisplay();
    Ui::Surface ui = surfaceFor(state.config);
    SCREENS[screenIndex].draw(ui, state, subIndex);
    ui.finish();
}

void drawCurrentScreen(const AppState& state) {
    drawScreen(currentScreen, state, currentSubScreen);
}

int getFirstEnabledScreen(const AppState& state) {
    for (int i = 0; i < NUM_SCREENS; i++) {
        int screenId = state.config.screen_order[i];
        if (isScreenEnabled(state, screenId)) {
            return screenId;
        }
    }
    return state.config.screen_order[0];
}

void jumpToFirstEnabledScreen(const AppState& state) {
    currentScreen = getFirstEnabledScreen(state);
}

void setContrast(bool dim) {
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(dim ? CONTRAST_DIM : CONTRAST_MAX);
}

int getNextAnimationEffect(uint16_t mask) {
    int enabledAnims[10]; int count = 0;
    for (int i = 1; i <= 5; i++) { if (mask & (1 << i)) enabledAnims[count++] = i; }
    if (count == 0) return 0;
    return enabledAnims[random(0, count)];
}

void animateHorizontal(int prev, int pSub, int next, int nSub, const AppState& state) {
    display.clearDisplay(); drawScreen(prev, state, pSub); memcpy(screenBufferOld, display.getBuffer(), 1024);
    display.clearDisplay(); drawScreen(next, state, nSub); memcpy(screenBufferNew, display.getBuffer(), 1024);
    int step = 8;
    for (int offset = 0; offset <= 128; offset += step) {
        uint8_t* displayBuf = display.getBuffer();
        for (int page = 0; page < 8; page++) {
            int start = page * 128;
            if (offset < 128) memcpy(&displayBuf[start], &screenBufferOld[start + offset], 128 - offset);
            if (offset > 0) memcpy(&displayBuf[start + (128 - offset)], &screenBufferNew[start], offset);
        }
        display.display();
    }
}

void animateVertical(int prev, int pSub, int next, int nSub, const AppState& state) {
    display.clearDisplay(); drawScreen(prev, state, pSub); memcpy(screenBufferOld, display.getBuffer(), 1024);
    display.clearDisplay(); drawScreen(next, state, nSub); memcpy(screenBufferNew, display.getBuffer(), 1024);
    for (int step = 0; step <= 8; step++) {
        uint8_t* displayBuf = display.getBuffer();
        for (int page = 0; page < 8; page++) {
            int oldPageIdx = page + step; int newPageIdx = page - (8 - step); int destIndex = page * 128;
            if (oldPageIdx < 8) memcpy(&displayBuf[destIndex], &screenBufferOld[oldPageIdx * 128], 128);
            else if (newPageIdx >= 0) memcpy(&displayBuf[destIndex], &screenBufferNew[newPageIdx * 128], 128);
        }
        display.display(); delay(10);
    }
}

void animateDissolve(int prev, int pSub, int next, int nSub, const AppState& state) {
    display.clearDisplay(); drawScreen(prev, state, pSub); memcpy(screenBufferOld, display.getBuffer(), 1024);
    display.clearDisplay(); drawScreen(next, state, nSub); memcpy(screenBufferNew, display.getBuffer(), 1024);
    uint8_t* displayBuf = display.getBuffer();
    for (int step = 0; step < 8; step++) {
        uint8_t mask = 0;
        switch (step) {
            case 0: mask = 0b10000000; break; case 1: mask = 0b11000000; break; case 2: mask = 0b11100000; break; case 3: mask = 0b11100100; break;
            case 4: mask = 0b11110100; break; case 5: mask = 0b11111100; break; case 6: mask = 0b11111110; break; case 7: mask = 0b11111111; break;
        }
        for (int i = 0; i < 1024; i++) displayBuf[i] = (screenBufferNew[i] & mask) | (screenBufferOld[i] & ~mask);
        display.display(); delay(10);
    }
}

void animateCurtain(int prev, int pSub, int next, int nSub, const AppState& state) {
    display.clearDisplay(); drawScreen(prev, state, pSub); memcpy(screenBufferOld, display.getBuffer(), 1024);
    display.clearDisplay(); drawScreen(next, state, nSub); memcpy(screenBufferNew, display.getBuffer(), 1024);
    int maxRadius = 80; int step = 4; uint8_t* displayBuf = display.getBuffer();
    for (int r = 0; r <= maxRadius; r += step) {
        int startX = 64 - r; if (startX < 0) startX = 0;
        int endX = 64 + r; if (endX > 128) endX = 128;
        for (int x = 0; x < 128; x++) {
            bool insideCurtain = (x >= startX && x < endX);
            for (int page = 0; page < 8; page++) {
                int idx = x + (page * 128); displayBuf[idx] = insideCurtain ? screenBufferNew[idx] : screenBufferOld[idx];
            }
        }
        display.display();
    }
}

void animateBlinds(int prev, int pSub, int next, int nSub, const AppState& state) {
    display.clearDisplay(); drawScreen(prev, state, pSub); memcpy(screenBufferOld, display.getBuffer(), 1024);
    display.clearDisplay(); drawScreen(next, state, nSub); memcpy(screenBufferNew, display.getBuffer(), 1024);
    uint8_t* displayBuf = display.getBuffer();
    memcpy(displayBuf, screenBufferOld, 1024); display.display();
    int blindWidth = 16; int numBlinds = 8; int stepSize = 2;
    for (int progress = 0; progress < blindWidth; progress += stepSize) {
        for (int blind = 0; blind < numBlinds; blind++) {
            int blindStartX = blind * blindWidth;
            for (int i = 0; i < stepSize; i++) {
                int currentX = blindStartX + progress + i; if (currentX >= 128) continue;
                for (int page = 0; page < 8; page++) {
                    int idx = currentX + (page * 128); displayBuf[idx] = screenBufferNew[idx];
                }
            }
        }
        display.display(); delay(5);
    }
}

void animateTransition(int prevScreen, int prevSub, int nextScreen, int nextSub, const AppState& state) {
    int selectedEffect = getNextAnimationEffect(state.config.anim_mask);
    switch (selectedEffect) {
        case ANIM_SLIDE_HORIZONTAL: animateHorizontal(prevScreen, prevSub, nextScreen, nextSub, state); break;
        case ANIM_SLIDE_VERTICAL: animateVertical(prevScreen, prevSub, nextScreen, nextSub, state); break;
        case ANIM_DISSOLVE: animateDissolve(prevScreen, prevSub, nextScreen, nextSub, state); break;
        case ANIM_CURTAIN: animateCurtain(prevScreen, prevSub, nextScreen, nextSub, state); break;
        case ANIM_BLINDS: animateBlinds(prevScreen, prevSub, nextScreen, nextSub, state); break;
        default:
            display.clearDisplay(); drawScreen(nextScreen, state, nextSub); display.display(); break;
    }
}

void switchToNextScreen(const AppState& state) {
    const Config& config = state.config;

    if (currentSubScreen + 1 < pageCount(state, currentScreen)) {
        currentSubScreen++;
        animateTransition(currentScreen, currentSubScreen - 1, currentScreen, currentSubScreen, state);
        return;
    }

    int oldScreen = currentScreen;
    int oldSubScreen = currentSubScreen;
    currentSubScreen = 0;

    int currentIndex = 0;
    for (int i = 0; i < NUM_SCREENS; i++) {
        if (config.screen_order[i] == currentScreen) {
            currentIndex = i; break;
        }
    }

    int checkIndex = currentIndex;
    int nextScreenCandidate = currentScreen;
    do {
        checkIndex++;
        if (checkIndex >= NUM_SCREENS) checkIndex = 0;
        int candidateId = config.screen_order[checkIndex];
        if (isScreenEnabled(state, candidateId)) {
            nextScreenCandidate = candidateId;
            break;
        }
    } while (checkIndex != currentIndex);

    if (oldScreen == nextScreenCandidate && oldSubScreen == 0) return;

    animateTransition(oldScreen, oldSubScreen, nextScreenCandidate, 0, state);
    currentScreen = nextScreenCandidate;
}

void switchToPreviousScreen(const AppState& state) {
    const Config& config = state.config;

    if (currentSubScreen > 0) {
        currentSubScreen--;
        animateTransition(currentScreen, currentSubScreen + 1, currentScreen, currentSubScreen, state);
        return;
    }

    int oldScreen = currentScreen;
    int oldSubScreen = currentSubScreen;

    int currentIndex = 0;
    for (int i = 0; i < NUM_SCREENS; i++) {
        if (config.screen_order[i] == currentScreen) {
            currentIndex = i; break;
        }
    }

    int checkIndex = currentIndex;
    int prevScreenCandidate = currentScreen;
    do {
        checkIndex--;
        if (checkIndex < 0) checkIndex = NUM_SCREENS - 1;
        int candidateId = config.screen_order[checkIndex];
        if (isScreenEnabled(state, candidateId)) {
            prevScreenCandidate = candidateId;
            break;
        }
    } while (checkIndex != currentIndex);

    if (oldScreen == prevScreenCandidate && oldSubScreen == 0) return;

    animateTransition(oldScreen, oldSubScreen, prevScreenCandidate, 0, state);
    currentScreen = prevScreenCandidate;
    currentSubScreen = 0;
}

void handleScreenNavigation(bool goToPrevious) {
    if (goToPrevious) {
        Serial.println("Double Click: Switching to Previous Screen");
        switchToPreviousScreen(appState);
    } else {
        Serial.println("Single Click: Switching to Next Screen");
        switchToNextScreen(appState);
    }
    lastScreenSwitch = millis();
}

void handleSingleClick() {
    handleScreenNavigation(false);
}

void handleDoubleClick() {
    handleScreenNavigation(true);
}

void handleLongPress() {
    appState.config.screen_auto_cycle = !appState.config.screen_auto_cycle;

    if (appState.config.screen_auto_cycle) {
        Serial.println("Auto Cycle: ENABLED");
        showAlert(icon_unlock, "Auto Cycle On");
    } else {
        Serial.println("Auto Cycle: DISABLED (Screen Locked)");
        showAlert(icon_lock, "Auto Cycle Off");
    }
    display.display();
    delay(1000);

    display.clearDisplay();
    drawCurrentScreen(appState);
    display.display();

    lastScreenSwitch = millis();
}

void setup() {
    Serial.begin(115200);
    delay(100);

    struct tm seedTime = {};
    seedTime.tm_year = 2026 - 1900;
    seedTime.tm_mon = 8;
    seedTime.tm_mday = 11;
    seedTime.tm_hour = 12;
    time_t seedEpoch = mktime(&seedTime);
    struct timeval tv = { seedEpoch, 0 };
    settimeofday(&tv, nullptr);

    Wire.begin(appState.config.sda_pin, appState.config.scl_pin);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 allocation failed");
        for (;;);
    }

    if (appState.config.button_type == "switch") {
        button.setup(appState.config.button_pin, INPUT_PULLUP, true);
    } else {
        button.setup(appState.config.button_pin, INPUT, false);
    }
    button.attachClick(handleSingleClick);
    button.attachDoubleClick(handleDoubleClick);
    button.attachLongPressStart(handleLongPress);
    button.setDebounceTicks(50);
    button.setClickTicks(150);
    button.setPressTicks(500);
    button.reset();

    jumpToFirstEnabledScreen(appState);
    display.clearDisplay();
    drawCurrentScreen(appState);
    display.display();
    lastScreenSwitch = millis();
}

void loop() {
    button.tick();

    if (appState.config.screen_auto_cycle) {
        unsigned long intervalMs = (unsigned long)appState.config.screen_interval_sec * 1000;
        if (millis() - lastScreenSwitch >= intervalMs) {
            switchToNextScreen(appState);
            lastScreenSwitch = millis();
        }
    }

    const ScreenDef& screen = SCREENS[currentScreen];
    bool animated = screen.animated && screen.animated(appState);
    if (animated && millis() - lastAnimatedFrame >= ANIMATED_REFRESH_MS) {
        lastAnimatedFrame = millis();
        drawCurrentScreen(appState);
        display.display();
    }
}
