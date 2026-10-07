#include "Screens.h"

#include "SaverScenes.h"
#include "ScreenHost.h"

void Screens::drawTime(Ui::Surface& ui, const AppState& state, int) {
  const Config& config = state.config;
  const String now = ScreenHost::clock(config.time_format);

  if (config.time_style == 1) {
    const String title = config.date_display ? ScreenHost::fullDate() : String("CLOCK");
    drawDesktopClock(ui.raw(), now.c_str(), title.c_str(), millis());
    return;
  }

  Ui::Canvas c = ui.frame("Clock", "", false);
  const int cx = c.w / 2;

  if (!config.date_display) {
    Ui::Font font = c.largestFit(now, c.w, Ui::FONT_GIANT);
    c.textCentered(now, cx, (c.h - c.textHeight(font)) / 2, font);
    return;
  }

  Ui::Font font = c.largestFit(now, c.w, Ui::FONT_HUGE);
  const int timeH = c.textHeight(font);
  const int y = (c.h - timeH - 6 - c.textHeight(Ui::FONT_NORMAL)) / 2;
  c.textCentered(now, cx, y, font);
  c.textFitCentered(ScreenHost::fullDate(), cx, y + timeH + 6, c.w);
}
