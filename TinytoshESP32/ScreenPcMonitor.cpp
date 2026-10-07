#include "Screens.h"

#include "images.h"

namespace {

const float NET_FULL_SCALE_KB = 5120.0f;

String netText(float kb) {
  if (kb >= 1024) return String((int)round(kb / 1024.0)) + "M";
  if (kb >= 100) return "<1M";
  return String((int)kb) + "K";
}

}  // namespace

bool Screens::pcHasData(const AppState& state) {
  const PcStats& pc = state.pc;
  const bool noCpu = isnan(pc.cpu_percent) || pc.cpu_percent == 0;
  const bool noRam = isnan(pc.mem_percent) || pc.mem_percent == 0;
  return !(noCpu && noRam);
}

void Screens::drawPcMonitor(Ui::Surface& ui, const AppState& state, int) {
  if (!pcHasData(state)) {
    ui.alert(icon_monitor, "No PC");
    return;
  }

  const PcStats& pc = state.pc;
  struct Row { const char* label; float percent; String value; };
  const Row rows[] = {
    {"CPU", pc.cpu_percent, String((int)round(pc.cpu_percent)) + "%"},
    {"RAM", pc.mem_percent, String((int)round(pc.mem_percent)) + "%"},
    {"DISK", pc.disk_percent, String((int)round(pc.disk_percent)) + "%"},
    {"NET", pc.net_down_kb / NET_FULL_SCALE_KB * 100.0f, netText(pc.net_down_kb)},
  };

  Ui::Canvas c = ui.frame("PC Monitor");
  for (int i = 0; i < 4; i++) {
    const int y = 3 + i * 12;
    c.text(rows[i].label, 0, y + 1, Ui::FONT_SMALL);
    c.progress(22, y, c.w - 50, 7, rows[i].percent);
    c.textRight(rows[i].value, c.w, y);
  }
}
