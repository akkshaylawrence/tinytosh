#include "Ui.h"

#include <Fonts/Picopixel.h>

namespace Ui {

namespace {

const uint16_t ON = 1;
const uint16_t OFF = 0;
const uint16_t INVERSE = 2;  // SSD1306_INVERSE

const int SMALL_HEIGHT = 5;
const int SMALL_BASELINE = 4;
const int ELLIPSIS_W = 6;
const int MAX_PARAGRAPH_LINES = 4;
const int GAP = 2;

const char* asciiFor(uint32_t cp) {
  switch (cp) {
    case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5:
    case 0x100: case 0x102: case 0x104: return "A";
    case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5:
    case 0x101: case 0x103: case 0x105: return "a";
    case 0xC7: case 0x106: case 0x108: case 0x10A: case 0x10C: return "C";
    case 0xE7: case 0x107: case 0x109: case 0x10B: case 0x10D: return "c";
    case 0xC8: case 0xC9: case 0xCA: case 0xCB:
    case 0x112: case 0x114: case 0x116: case 0x118: case 0x11A: return "E";
    case 0xE8: case 0xE9: case 0xEA: case 0xEB:
    case 0x113: case 0x115: case 0x117: case 0x119: case 0x11B: return "e";
    case 0xCC: case 0xCD: case 0xCE: case 0xCF:
    case 0x128: case 0x12A: case 0x12C: case 0x12E: case 0x130: return "I";
    case 0xEC: case 0xED: case 0xEE: case 0xEF:
    case 0x129: case 0x12B: case 0x12D: case 0x12F: case 0x131: return "i";
    case 0xD1: case 0x143: case 0x145: case 0x147: return "N";
    case 0xF1: case 0x144: case 0x146: case 0x148: return "n";
    case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD8:
    case 0x14C: case 0x14E: case 0x150: return "O";
    case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF8:
    case 0x14D: case 0x14F: case 0x151: return "o";
    case 0xD9: case 0xDA: case 0xDB: case 0xDC:
    case 0x168: case 0x16A: case 0x16C: case 0x16E: case 0x170: case 0x172: return "U";
    case 0xF9: case 0xFA: case 0xFB: case 0xFC:
    case 0x169: case 0x16B: case 0x16D: case 0x16F: case 0x171: case 0x173: return "u";
    case 0xDD: case 0x176: case 0x178: return "Y";
    case 0xFD: case 0xFF: case 0x177: return "y";
    case 0xD0: return "D";
    case 0xF0: return "d";
    case 0xDE: return "Th";
    case 0xFE: return "th";
    case 0xDF: return "ss";
    case 0xC6: return "AE";
    case 0xE6: return "ae";
    case 0x152: return "OE";
    case 0x153: return "oe";
    case 0x160: return "S";
    case 0x161: return "s";
    case 0x179: case 0x17B: case 0x17D: return "Z";
    case 0x17A: case 0x17C: case 0x17E: return "z";
    case 0x141: return "L";
    case 0x142: return "l";
    case 0x158: return "R";
    case 0x159: return "r";
    case 0x164: return "T";
    case 0x165: return "t";
    case 0x11E: return "G";
    case 0x11F: return "g";
    default: return "";
  }
}

}  // namespace

Style styleFrom(const String& chrome, const String& paper) {
  Style style;
  style.chrome = chrome == "menubar" ? CHROME_MENUBAR : CHROME_WINDOW;
  style.whitePaper = paper == "white";
  return style;
}

String ascii(const String& utf8) {
  String out;
  out.reserve(utf8.length());

  for (size_t i = 0; i < utf8.length();) {
    uint8_t c = (uint8_t)utf8[i];
    if (c < 0x80) {
      out += (char)c;
      i++;
      continue;
    }

    uint32_t codepoint = 0;
    int extraBytes;
    if ((c & 0xE0) == 0xC0) { codepoint = c & 0x1F; extraBytes = 1; }
    else if ((c & 0xF0) == 0xE0) { codepoint = c & 0x0F; extraBytes = 2; }
    else if ((c & 0xF8) == 0xF0) { codepoint = c & 0x07; extraBytes = 3; }
    else { i++; continue; }

    size_t seqLen = 1;
    bool valid = true;
    for (int b = 0; b < extraBytes; b++) {
      if (i + seqLen >= utf8.length()) { valid = false; break; }
      uint8_t cont = (uint8_t)utf8[i + seqLen];
      if ((cont & 0xC0) != 0x80) { valid = false; break; }
      codepoint = (codepoint << 6) | (cont & 0x3F);
      seqLen++;
    }
    if (!valid) { i++; continue; }

    out += asciiFor(codepoint);
    i += seqLen;
  }
  return out;
}

Canvas::Canvas(Adafruit_GFX& g, int x, int y, int w, int h, bool whitePaper)
  : w(w), h(h), g_(g), x_(x), y_(y), whitePaper_(whitePaper) {}

void Canvas::useFont(Font f) {
  g_.setTextWrap(false);
  g_.setTextColor(ink_);
  if (f == FONT_SMALL) {
    g_.setFont(&Picopixel);
    g_.setTextSize(1);
  } else {
    g_.setFont();
    g_.setTextSize((int)f);
  }
}

int Canvas::textWidth(const String& s, Font f) {
  if (s.length() == 0) return 0;
  useFont(f);
  int16_t x1, y1;
  uint16_t tw, th;
  if (f == FONT_SMALL) {
    String upper = s;
    upper.toUpperCase();
    g_.getTextBounds(upper.c_str(), 0, 0, &x1, &y1, &tw, &th);
    return tw;
  }
  g_.getTextBounds(s.c_str(), 0, 0, &x1, &y1, &tw, &th);
  return (int)tw - (int)f;
}

int Canvas::textHeight(Font f) const {
  return f == FONT_SMALL ? SMALL_HEIGHT : 7 * (int)f;
}

Font Canvas::largestFit(const String& s, int maxW, Font largest) {
  int f = largest;
  while (f > FONT_NORMAL && textWidth(s, (Font)f) > maxW) f--;
  return (Font)f;
}

void Canvas::text(const String& s, int x, int y, Font f) {
  if (s.length() == 0) return;
  Group element(s.c_str());
  useFont(f);
  if (f == FONT_SMALL) {
    String upper = s;
    upper.toUpperCase();
    g_.setCursor(x_ + x, y_ + y + SMALL_BASELINE);
    g_.print(upper);
  } else {
    g_.setCursor(x_ + x, y_ + y);
    g_.print(s);
  }
}

void Canvas::textCentered(const String& s, int cx, int y, Font f) {
  text(s, cx - textWidth(s, f) / 2, y, f);
}

void Canvas::textRight(const String& s, int rightX, int y, Font f) {
  text(s, rightX - textWidth(s, f), y, f);
}

int Canvas::textWithMark(const String& s, int x, int y, Font f, const uint8_t* mark, int markW, int markH) {
  Group element(s.c_str());
  const int tw = textWidth(s, f);
  text(s, x, y, f);
  icon(mark, x + tw + 1, y, markW, markH);
  return tw + 1 + markW;
}

String Canvas::truncated(const String& s, int maxW, Font f, bool& cut) {
  cut = textWidth(s, f) > maxW;
  if (!cut) return s;
  String t = s;
  while (t.length() > 0 && textWidth(t, f) + ELLIPSIS_W > maxW) t.remove(t.length() - 1);
  t.trim();
  return t;
}

int Canvas::textFit(const String& s, int x, int y, int maxW, Font f) {
  bool cut;
  String t = truncated(s, maxW, f, cut);
  if (!cut) {
    text(t, x, y, f);
    return textWidth(t, f);
  }
  Group element(s.c_str());
  text(t, x, y, f);
  const int drawn = textWidth(t, f);
  const int dotY = y + textHeight(f) - 1;
  for (int i = 1; i < ELLIPSIS_W; i += 2) pixel(x + drawn + i, dotY, ink_);
  return drawn + ELLIPSIS_W;
}

void Canvas::textFitCentered(const String& s, int cx, int y, int maxW, Font f) {
  int tw = textWidth(s, f);
  if (tw > maxW) tw = maxW;
  textFit(s, cx - tw / 2, y, maxW, f);
}

void Canvas::textFitRight(const String& s, int rightX, int y, int maxW, Font f) {
  int tw = textWidth(s, f);
  if (tw > maxW) tw = maxW;
  textFit(s, rightX - tw, y, maxW, f);
}

void Canvas::paragraph(const String& s, int x, int& y, int maxW, Font f, int maxLines) {
  if (s.length() == 0) return;
  if (maxLines > MAX_PARAGRAPH_LINES) maxLines = MAX_PARAGRAPH_LINES;
  if (maxLines < 1) maxLines = 1;

  String lines[MAX_PARAGRAPH_LINES];
  int count = 0;
  unsigned int start = 0;

  while (start < s.length()) {
    int space = s.indexOf(' ', start);
    if (space == -1) space = s.length();
    String word = s.substring(start, space);
    String candidate = lines[count].length() == 0 ? word : lines[count] + " " + word;

    if (textWidth(candidate, f) <= maxW || lines[count].length() == 0) {
      lines[count] = candidate;
    } else if (count + 1 < maxLines) {
      lines[++count] = word;
    } else {
      lines[count] = candidate;
      break;
    }
    start = space + 1;
  }

  // One row for descenders, then the gap.
  const int step = textHeight(f) + (f == FONT_SMALL ? 0 : 1) + GAP;
  for (int i = 0; i <= count; i++) {
    textFit(lines[i], x, y, maxW, f);
    y += step;
  }
}

void Canvas::icon(const uint8_t* bitmap, int x, int y, int w, int h) {
  Group element("icon");
  g_.drawBitmap(x_ + x, y_ + y, bitmap, w, h, ink_);
}

void Canvas::pixel(int x, int y, uint16_t color) {
  Group element("pixel");
  g_.drawPixel(x_ + x, y_ + y, color);
}

void Canvas::hline(int x, int y, int w) {
  Group element("rule", KIND_DIVIDER);
  g_.drawFastHLine(x_ + x, y_ + y, w, ink_);
}

void Canvas::vline(int x, int y, int h) {
  Group element("rule", KIND_DIVIDER);
  g_.drawFastVLine(x_ + x, y_ + y, h, ink_);
}

void Canvas::dotted(int x, int y, int w) {
  Group element("rule", KIND_DIVIDER);
  for (int i = 0; i < w; i += 2) g_.drawPixel(x_ + x + i, y_ + y, ink_);
}

void Canvas::rect(int x, int y, int w, int h) {
  Group element("box");
  g_.drawRect(x_ + x, y_ + y, w, h, ink_);
}

void Canvas::fill(int x, int y, int w, int h) {
  Group element("box");
  g_.fillRect(x_ + x, y_ + y, w, h, ink_);
}

void Canvas::invert(int x, int y, int w, int h) { g_.fillRect(x_ + x, y_ + y, w, h, INVERSE); }

void Canvas::progress(int x, int y, int w, int h, float percent) {
  if (isnan(percent) || percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  Group element("progress bar");
  rect(x, y, w, h);
  int filled = (int)((w - 4) * percent / 100.0f);
  if (filled > 0) fill(x + 2, y + 2, filled, h - 4);
}

int Canvas::statWidth(const Stat& s, Font f, bool withUnit) {
  return 8 + GAP + textWidth(s.text, f) + (withUnit && s.unit ? 1 + s.unitW : 0);
}

// A stat too wide for its slot loses its unit before anything else. If a value still does not
// fit, the whole set drops to the small face together, so one row never mixes sizes.
Font Canvas::statFont(const Stat* stats, int count, int slotW) {
  for (int i = 0; i < count; i++) {
    if (statWidth(stats[i], FONT_NORMAL, false) > slotW) return FONT_SMALL;
  }
  return FONT_NORMAL;
}

int Canvas::statFitWidth(const Stat& s, Font f, int slotW) {
  if (statWidth(s, f, true) <= slotW) return statWidth(s, f, true);
  const int bare = statWidth(s, f, false);
  return bare < slotW ? bare : slotW;
}

void Canvas::stat(const Stat& s, int x, int y, Font f, int slotW) {
  Group element(s.text.c_str());
  icon(s.icon, x, y, 8, 8);
  const int textX = x + 8 + GAP;
  const int textY = y + (f == FONT_SMALL ? 2 : 1);
  if (s.unit && statWidth(s, f, true) <= slotW) textWithMark(s.text, textX, textY, f, s.unit, s.unitW, s.unitH);
  else textFit(s.text, textX, textY, slotW - 8 - GAP, f);
}

void Canvas::statRow(const Stat* stats, int count, int y) {
  if (count > 3) count = 3;
  const int slotW = (w - 2 * GAP) / 3;
  const Font f = statFont(stats, count, slotW);
  for (int i = 0; i < count; i++) {
    const int sw = statFitWidth(stats[i], f, slotW);
    const int x = i == 0 ? 0 : i == 1 ? (w - sw) / 2 : w - sw;
    stat(stats[i], x, y, f, slotW);
  }
}

void Canvas::statGrid(const Stat* stats, int count, int x, int w) {
  if (count <= 0) return;
  const int cols = count > 3 ? 2 : 1;
  const int rows = (count + cols - 1) / cols;
  const int pitch = (w + GAP) / cols;
  const int slotW = pitch - GAP;
  const int rowH = h / rows;
  const Font f = statFont(stats, count, slotW);
  for (int i = 0; i < count; i++) {
    int cx = x + (i % cols) * pitch;
    if (cols == 1) cx += (slotW - statFitWidth(stats[i], f, slotW)) / 2;
    stat(stats[i], cx, (i / cols) * rowH + (rowH - 8) / 2, f, slotW);
  }
}

Surface::Surface(Adafruit_GFX& g, Style style, const String& clock)
  : g_(g), style_(style), clock_(clock) {}

void Surface::menuBar(const String& title, bool showClock) {
  g_.fillRect(0, 0, SCREEN_W, 9, ON);
  g_.fillRect(3, 3, 4, 4, OFF);
  g_.drawPixel(5, 2, OFF);

  Canvas bar(g_, 0, 0, SCREEN_W, 9, false);
  bar.ink_ = OFF;
  const String clock = showClock ? clock_ : String("");
  int clockW = bar.textWidth(clock, FONT_SMALL);
  bar.textRight(clock, SCREEN_W - 2, 2, FONT_SMALL);
  bar.textFit(title, 12, 2, SCREEN_W - 12 - clockW - 8, FONT_SMALL);
}

Canvas Surface::frame(const String& title, const String& corner, bool showClock) {
  Group chrome("frame", KIND_CHROME);
  if (style_.chrome == CHROME_MENUBAR) {
    menuBar(title, showClock);
    paperY_ = 10;
    return Canvas(g_, CONTENT_X, CONTENT_Y, CONTENT_W, CONTENT_H, style_.whitePaper);
  }

  g_.drawRect(0, 0, SCREEN_W, SCREEN_H, ON);
  for (int y = 2; y <= 6; y += 2) g_.drawFastHLine(2, y, SCREEN_W - 4, ON);
  g_.drawFastHLine(0, 8, SCREEN_W, ON);
  g_.fillRect(7, 1, 9, 7, OFF);
  g_.drawRect(9, 2, 5, 5, ON);

  Canvas bar(g_, 0, 0, SCREEN_W, 9, false);
  int cornerW = bar.textWidth(corner, FONT_SMALL);
  if (cornerW > 0) {
    g_.fillRect(SCREEN_W - 7 - cornerW, 1, cornerW + 5, 7, OFF);
    bar.textRight(corner, SCREEN_W - 4, 2, FONT_SMALL);
  }
  // A centred title may only grow as far as the nearer of the close box and the corner text.
  const int side = cornerW > 0 ? cornerW + 12 : 22;
  const int maxTitleW = SCREEN_W - 2 * side;
  int titleW = bar.textWidth(title, FONT_SMALL);
  if (titleW > maxTitleW) titleW = maxTitleW;
  const int titleX = (SCREEN_W - titleW) / 2;
  g_.fillRect(titleX - 3, 1, titleW + 6, 7, OFF);
  bar.textFit(title, titleX, 2, maxTitleW, FONT_SMALL);

  paperY_ = 0;
  return Canvas(g_, CONTENT_X, CONTENT_Y, CONTENT_W, CONTENT_H, style_.whitePaper);
}

void Surface::alert(const uint8_t* icon32, const String& message) {
  int top = 0;
  {
    Group chrome("alert box", KIND_CHROME);
    if (style_.chrome == CHROME_MENUBAR) {
      menuBar("", true);
      top = 11;
      paperY_ = 10;
    } else {
      paperY_ = 0;
    }
    const int boxH = SCREEN_H - top;
    g_.drawRect(0, top, SCREEN_W, boxH, ON);
    g_.drawRect(2, top + 2, SCREEN_W - 4, boxH - 4, ON);
    g_.drawRect(3, top + 3, SCREEN_W - 6, boxH - 6, ON);
  }

  Canvas c(g_, 6, top + 6, SCREEN_W - 12, SCREEN_H - top - 12, style_.whitePaper);
  const int iconY = (c.h - 32) / 2;
  if (icon32) c.icon(icon32, 4, iconY, 32, 32);

  int textY = iconY + 1;
  c.paragraph(message, 44, textY, c.w - 46, FONT_NORMAL, 2);

  Group button("OK button");
  const int bw = 26, bh = 11, bx = c.w - bw, by = c.h - bh;
  c.rect(bx, by, bw, bh);
  c.pixel(bx, by, OFF);
  c.pixel(bx + bw - 1, by, OFF);
  c.pixel(bx, by + bh - 1, OFF);
  c.pixel(bx + bw - 1, by + bh - 1, OFF);
  c.textCentered("OK", bx + bw / 2 + 1, by + 2);
}

void Surface::startup(const uint8_t* face24, const String& headline, const String& detail, int percent) {
  Canvas c = frame("Tinytosh", "", false);
  const int cx = c.w / 2;
  c.icon(face24, cx - 12, 0, 24, 24);
  c.textFitCentered(headline, cx, 27, c.w);
  c.textFitCentered(detail, cx, 37, c.w, FONT_SMALL);
  if (percent >= 0) c.progress(10, 44, c.w - 20, 6, percent);
}

void Surface::notice(const String& title, const String* lines, int count) {
  const int lineH = 10, maxLines = 5;
  if (count > maxLines) count = maxLines;
  Canvas c = frame(title, "", false);
  int y = (c.h - count * lineH + GAP) / 2;
  for (int i = 0; i < count; i++, y += lineH) c.textFitCentered(lines[i], c.w / 2, y, c.w);
}

void Surface::finish() {
  if (style_.whitePaper && paperY_ >= 0) g_.fillRect(0, paperY_, SCREEN_W, SCREEN_H - paperY_, INVERSE);
  paperY_ = -1;
}

}  // namespace Ui
