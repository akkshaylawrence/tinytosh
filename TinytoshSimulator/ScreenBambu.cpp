#include "Screens.h"

#include "images.h"

namespace {

void drawReading(Ui::Canvas& c, const unsigned char* icon, float degrees, int x, int y) {
  const String value = String((int)round(degrees));
  Ui::Group reading(value.c_str());
  c.icon(icon, x, y + 3, 8, 8);
  c.textWithMark(value, x + 12, y, Ui::FONT_LARGE, degree_icon_small, 4, 4);
}

void drawIdle(Ui::Canvas& c, const BambuData& data, const String& status) {
  c.icon(icon_printer, 0, 9, 32, 32);
  const int x = 40;
  c.textFit(status, x, 1, c.w - x);
  drawReading(c, icon_nozzle, data.nozzle_temp, x, 15);
  drawReading(c, icon_bed, data.bed_temp, x, 34);
}

void drawGauge(Ui::Canvas& c, const unsigned char* icon, const String& value, int y, bool rightSide) {
  Ui::Group gauge(value.c_str());
  if (rightSide) {
    c.icon(icon, c.w - 8, y, 8, 8);
    c.textRight(value, c.w - 11, y + 1);
  } else {
    c.icon(icon, 0, y, 8, 8);
    c.text(value, 11, y + 1);
  }
}

void drawPrinting(Ui::Canvas& c, const BambuData& data) {
  drawGauge(c, icon_nozzle, String((int)round(data.nozzle_temp)) + "/" + String((int)round(data.nozzle_target)), 0, false);
  drawGauge(c, icon_bed, String((int)round(data.bed_temp)) + "/" + String((int)round(data.bed_target)), 10, false);
  drawGauge(c, icon_part_fan, String(data.fan_part) + "%", 0, true);
  drawGauge(c, icon_aux_fan, String(data.fan_aux) + "%", 10, true);

  const bool unnamed = data.file_name.length() == 0 || data.file_name.equalsIgnoreCase("None");
  c.textFitCentered(unnamed ? String("Idle") : Ui::ascii(data.file_name), c.w / 2, 21, c.w);

  c.progress(0, 32, c.w, 6, data.progress);
  c.text(String(data.progress) + "%", 0, 41);
  c.textCentered(String(data.layer) + "/" + String(data.total_layers), c.w / 2, 41);
  c.textRight(String(data.time_left) + "m", c.w, 41);
}

}  // namespace

bool Screens::bambuHasData(const AppState& state) {
  return state.bambu.status != "SYNCING" && state.bambu.status.length() > 0;
}

void Screens::drawBambu(Ui::Surface& ui, const AppState& state, int) {
  if (!bambuHasData(state)) {
    ui.alert(icon_printer, "No Printer");
    return;
  }

  const BambuData& data = state.bambu;
  String status = data.status;
  status.toUpperCase();
  const bool idle = status == "IDLE" || status == "FINISH" || status == "FINISHED" || status == "FAILED";

  Ui::Canvas c = ui.frame(idle ? "Printer" : "Printing");
  if (idle) drawIdle(c, data, status);
  else drawPrinting(c, data);
}
