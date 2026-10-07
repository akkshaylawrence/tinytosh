#include "Screens.h"

#include "images.h"
#include "ScreenHost.h"

namespace {

void drawDisc(Ui::Canvas& c, int cx, int cy, int r, float illuminated, bool waxing) {
  Ui::Group disc("moon");
  for (int dy = -r; dy <= r; dy++) {
    const float halfWidth = sqrtf((float)(r * r - dy * dy));
    const float terminator = halfWidth * (1.0f - 2.0f * illuminated);
    for (int dx = -r; dx <= r; dx++) {
      const int d2 = dx * dx + dy * dy;
      if (d2 > r * r + r) continue;
      const bool rim = d2 > (r - 1) * (r - 1) + (r - 1);
      const bool lit = waxing ? dx >= terminator : dx <= -terminator;
      c.pixel(cx + dx, cy + dy, rim ? 1 : c.photoColor(lit));
    }
  }
}

void drawEvent(Ui::Canvas& c, const unsigned char* arrow, const String& time, int x, int y) {
  Ui::Group event(time.c_str());
  c.icon(arrow, x, y + 2, 7, 4);
  c.text(time, x + 10, y);
}

String moonTime(int mins, const String& timeFormat) {
  return mins != -1 ? ScreenHost::formatMinsFromMidnight(mins, timeFormat, true) : String("--:--");
}

}  // namespace

void Screens::drawMoon(Ui::Surface& ui, const AppState& state, int) {
  const Config& config = state.config;
  const MoonData& data = state.moon;
  if (data.fracillum == -1) {
    ui.alert(icon_error, "No Moon Data");
    return;
  }

  const float illuminated = data.fracillum / 100.0f;
  const bool waxing = data.curphase.indexOf("Wax") >= 0 || data.curphase.indexOf("First") >= 0 || data.curphase.indexOf("New") >= 0;
  Ui::Canvas c = ui.frame("Moon");

  if (config.moon_minimal) {
    drawDisc(c, c.w / 2, 18, 18, illuminated, waxing);
    c.textFitCentered(data.curphase, c.w / 2, 41, c.w);
    return;
  }

  drawDisc(c, 22, 24, 22, illuminated, waxing);
  const int x = 50;
  int y = 0;
  c.paragraph(data.curphase, x, y, c.w - x, Ui::FONT_NORMAL, 2);
  c.text(String(data.fracillum) + "% lit", x, 22, Ui::FONT_SMALL);
  drawEvent(c, icon_up, moonTime(data.rise_mins, config.time_format), x, 30);
  drawEvent(c, icon_down, moonTime(data.set_mins, config.time_format), x, 40);
}
