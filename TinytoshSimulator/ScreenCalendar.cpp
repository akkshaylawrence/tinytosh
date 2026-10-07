#include "Screens.h"

#include "ScreenHost.h"

namespace {

const char* const MONTHS[] = {"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
const char* const WEEKDAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

const int GRID_X = 59;
const int CELL_W = 9;

int daysIn(int month, int year) {
  const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  return month == 1 && leap ? 29 : days[month];
}

void drawGrid(Ui::Canvas& c, const struct tm& now, bool mondayFirst) {
  const int numDays = daysIn(now.tm_mon, now.tm_year + 1900);
  const int firstWeekday = (now.tm_wday - ((now.tm_mday - 1) % 7) + 7) % 7;
  const int startCol = mondayFirst ? (firstWeekday + 6) % 7 : firstWeekday;
  const int weeks = (startCol + numDays + 6) / 7;
  const int rowH = weeks == 6 ? 7 : 8;
  const int top = weeks == 6 ? 7 : 9;

  const char* headers = mondayFirst ? "MTWTFSS" : "SMTWTFS";
  for (int i = 0; i < 7; i++) c.textCentered(String(headers[i]), GRID_X + i * CELL_W + 4, 0, Ui::FONT_SMALL);

  for (int day = 1; day <= numDays; day++) {
    const int cell = startCol + day - 1;
    const int x = GRID_X + (cell % 7) * CELL_W;
    const int y = top + (cell / 7) * rowH;
    c.textCentered(String(day), x + 4, y, Ui::FONT_SMALL);
    if (day == now.tm_mday) c.invert(x, y - 1, CELL_W, 7);
  }
}

}  // namespace

void Screens::drawCalendar(Ui::Surface& ui, const AppState& state, int) {
  const struct tm now = ScreenHost::localNow();
  const String year = String(now.tm_year + 1900);
  Ui::Canvas c = ui.frame("Calendar");

  if (state.config.calendar_minimal) {
    const int cx = c.w / 2;
    c.textCentered(WEEKDAYS[now.tm_wday], cx, 0);
    c.textCentered(String(now.tm_mday), cx, 10, Ui::FONT_GIANT);
    c.textCentered(String(MONTHS[now.tm_mon]) + " " + year, cx, 41);
    return;
  }

  const int colW = GRID_X - 5;
  const int cx = colW / 2;
  c.textCentered(WEEKDAYS[now.tm_wday], cx, 0, Ui::FONT_SMALL);
  c.textCentered(String(now.tm_mday), cx, 8, Ui::FONT_HUGE);
  c.textFitCentered(MONTHS[now.tm_mon], cx, 32, colW);
  c.textCentered(year, cx, 43, Ui::FONT_SMALL);
  c.vline(colW + 2, 0, c.h);

  drawGrid(c, now, state.config.calendar_start_day == "mon");
}
