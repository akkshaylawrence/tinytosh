#ifndef UI_H
#define UI_H

// Classic Mac look for the 128x64 OLED.
//
// A screen asks a Surface for a framed Canvas and draws lit pixels on it. The Surface owns the
// chrome (window or menu bar) and the paper colour, so a screen never branches on either.
// Every frame gives the same 122x50 content area, so one layout serves every style.
//
// Spacing rule: 2 clear pixels between any content and the frame, a divider, or other content.
// The content area already sits 2 pixels inside the frame, so a screen may draw right to its
// edges. tools/ui-harness enforces the rule on every screen.

#include <Arduino.h>
#include <Adafruit_GFX.h>

namespace Ui {

const int SCREEN_W = 128;
const int SCREEN_H = 64;
const int CONTENT_W = 122;
const int CONTENT_H = 50;
const int CONTENT_X = 3;
const int CONTENT_Y = 11;

enum Chrome : uint8_t { CHROME_WINDOW, CHROME_MENUBAR };

struct Style {
  Chrome chrome = CHROME_WINDOW;
  bool whitePaper = false;
};

Style styleFrom(const String& chrome, const String& paper);

// SMALL is the 5 px label face and always prints upper case. The rest are the 5x7 face at 1x to 4x.
enum Font : uint8_t { FONT_SMALL, FONT_NORMAL, FONT_LARGE, FONT_HUGE, FONT_GIANT };

// Each drawing call is one element to the spacing check. A Group makes several calls count as
// one, for parts that belong together: an arrow and its label, a picture built from pixels.
// On the device a Group does nothing.
enum Kind : uint8_t { KIND_CONTENT, KIND_DIVIDER, KIND_CHROME };

#ifdef UI_LAYOUT_AUDIT
void auditBegin(Kind kind, const char* label);
void auditEnd();
#else
inline void auditBegin(Kind, const char*) {}
inline void auditEnd() {}
#endif

class Group {
public:
  explicit Group(const char* label, Kind kind = KIND_CONTENT) { auditBegin(kind, label); }
  ~Group() { auditEnd(); }
  Group(const Group&) = delete;
  Group& operator=(const Group&) = delete;
};

struct Stat {
  const uint8_t* icon;           // 8x8
  String text;
  const uint8_t* unit = nullptr; // optional bitmap drawn right after the text
  uint8_t unitW = 0;
  uint8_t unitH = 0;
};

class Canvas {
public:
  Canvas(Adafruit_GFX& g, int x, int y, int w, int h, bool whitePaper);

  const int w;
  const int h;

  // Pictures such as the moon must keep their lit side lit on white paper. Draw those pixels
  // with photoColor(lit) instead of plain on and off.
  uint16_t photoColor(bool lit) const { return lit != whitePaper_ ? 1 : 0; }

  int textWidth(const String& s, Font f = FONT_NORMAL);
  int textHeight(Font f) const;
  Font largestFit(const String& s, int maxW, Font largest);

  void text(const String& s, int x, int y, Font f = FONT_NORMAL);
  void textCentered(const String& s, int cx, int y, Font f = FONT_NORMAL);
  void textRight(const String& s, int rightX, int y, Font f = FONT_NORMAL);
  // Text with a small bitmap right after it, such as a degree mark. Returns the width drawn.
  int textWithMark(const String& s, int x, int y, Font f, const uint8_t* mark, int markW, int markH);
  // Truncates with an ellipsis when s is wider than maxW. Returns the width drawn.
  int textFit(const String& s, int x, int y, int maxW, Font f = FONT_NORMAL);
  void textFitCentered(const String& s, int cx, int y, int maxW, Font f = FONT_NORMAL);
  void textFitRight(const String& s, int rightX, int y, int maxW, Font f = FONT_NORMAL);
  // Word-wraps into at most maxLines and moves y below the last line.
  void paragraph(const String& s, int x, int& y, int maxW, Font f, int maxLines);

  void icon(const uint8_t* bitmap, int x, int y, int w, int h);
  void pixel(int x, int y, uint16_t color = 1);
  void hline(int x, int y, int w);
  void vline(int x, int y, int h);
  void dotted(int x, int y, int w);
  void rect(int x, int y, int w, int h);
  void fill(int x, int y, int w, int h);
  void invert(int x, int y, int w, int h);
  void progress(int x, int y, int w, int h, float percent);

  // Up to three icon+value pairs spread across the full width.
  void statRow(const Stat* stats, int count, int y);
  // Icon+value pairs filling the box top to bottom, in one column up to three and two beyond.
  void statGrid(const Stat* stats, int count, int x, int w);

private:
  Adafruit_GFX& g_;
  const int x_;
  const int y_;
  const bool whitePaper_;
  uint16_t ink_ = 1;

  friend class Surface;

  void useFont(Font f);
  String truncated(const String& s, int maxW, Font f, bool& cut);
  int statWidth(const Stat& s, Font f, bool withUnit);
  Font statFont(const Stat* stats, int count, int slotW);
  int statFitWidth(const Stat& s, Font f, int slotW);
  void stat(const Stat& s, int x, int y, Font f, int slotW);
};

class Surface {
public:
  Surface(Adafruit_GFX& g, Style style, const String& clock);

  // The standard frame. corner is extra text for the window title bar. The menu bar shows the
  // clock there on its own, unless the screen is itself a clock or the time is not yet known.
  Canvas frame(const String& title, const String& corner = "", bool showClock = true);
  // A Mac alert box with a 32x32 icon. Use for empty and error states.
  void alert(const uint8_t* icon32, const String& message);
  // The boot card: a 24x24 Mac face, a headline, a detail line, and a progress bar.
  // A negative percent leaves the bar out.
  void startup(const uint8_t* face24, const String& headline, const String& detail, int percent);
  // A framed window of up to five centred lines, for values the user has to read off.
  void notice(const String& title, const String* lines, int count);
  // For scenes that paint the whole panel themselves. Paper colour is not applied to them.
  Adafruit_GFX& raw() { return g_; }
  // Applies the paper colour. Call once, after the screen has drawn.
  void finish();

private:
  Adafruit_GFX& g_;
  const Style style_;
  const String clock_;
  int paperY_ = -1;

  void menuBar(const String& title, bool showClock);
};

// Folds accented Latin letters to ASCII and drops anything else the fonts cannot draw.
String ascii(const String& utf8);

}  // namespace Ui

#endif
