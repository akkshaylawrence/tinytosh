#include "Screens.h"

#include "images.h"

namespace {

enum Phase : uint8_t { PHASE_IDLE, PHASE_PREPARING, PHASE_PRINTING, PHASE_PAUSED, PHASE_FINISHED, PHASE_FAILED };

Phase phaseOf(String status) {
  status.toUpperCase();
  if (status == "IDLE") return PHASE_IDLE;
  if (status == "PREPARE") return PHASE_PREPARING;
  if (status == "PAUSE") return PHASE_PAUSED;
  if (status == "FINISH" || status == "FINISHED") return PHASE_FINISHED;
  if (status == "FAILED") return PHASE_FAILED;
  return PHASE_PRINTING;
}

// Never wider than five characters. Past ten hours the minutes stop mattering.
String formatTimeLeft(int mins) {
  if (mins < 60) return String(mins) + "m";
  if (mins >= 600) return String(mins / 60) + "h";
  const int rest = mins % 60;
  return String(mins / 60) + "h" + (rest < 10 ? "0" : "") + String(rest) + "m";
}

void drawRow(Ui::Canvas& c, const String& label, const String& value, int y) {
  c.text(label, 0, y + 4, Ui::FONT_SMALL);
  c.textRight(value, c.w, y, Ui::FONT_LARGE);
}

void drawTempRow(Ui::Canvas& c, const String& label, float degrees, int y) {
  const String value = String((int)round(degrees));
  c.text(label, 0, y + 4, Ui::FONT_SMALL);
  c.textWithMark(value, c.w - c.textWidth(value, Ui::FONT_LARGE) - 5, y, Ui::FONT_LARGE, degree_icon_small, 4, 4);
}

void drawProgress(Ui::Canvas& c, const String& label, int percent, int y) {
  percent = percent < 0 ? 0 : percent > 100 ? 100 : percent;
  Ui::Group bar("progress bar");
  c.rect(0, y, c.w, 11);
  c.textCentered(label, c.w / 2, y + 2);
  // Fills past the digits' height, so inverted digits keep a lit row above and below them.
  c.invert(1, y + 1, (c.w - 2) * percent / 100, 9);
}

void drawFans(Ui::Canvas& c, const BambuData& data, int y) {
  c.textRight("F" + String(data.fan_part) + " A" + String(data.fan_aux), c.w, y, Ui::FONT_SMALL);
}

void drawIdle(Ui::Canvas& c, const BambuData& data) {
  drawTempRow(c, "Noz", data.nozzle_temp, 0);
  drawTempRow(c, "Bed", data.bed_temp, 16);
  drawProgress(c, "Idle", 0, 32);
  drawFans(c, data, 45);
}

void drawJob(Ui::Canvas& c, const BambuData& data, Phase phase) {
  if (phase == PHASE_FINISHED) drawRow(c, "Print", "Done", 0);
  else if (phase == PHASE_FAILED) drawRow(c, "Print", "Failed", 0);
  else drawRow(c, "Left", formatTimeLeft(data.time_left), 0);
  drawRow(c, "Lyr", String(data.layer) + "/" + String(data.total_layers), 16);

  const String percent = String(data.progress) + "%";
  drawProgress(c, phase == PHASE_PREPARING ? String("Preparing") : phase == PHASE_PAUSED ? String("Paused") : percent, data.progress, 32);

  c.text("N" + String((int)round(data.nozzle_temp)) + " B" + String((int)round(data.bed_temp)), 0, 45, Ui::FONT_SMALL);
  drawFans(c, data, 45);
}

String jobTitle(const BambuData& data) {
  const bool unnamed = data.file_name.length() == 0 || data.file_name.equalsIgnoreCase("None");
  return unnamed ? String("Printer") : Ui::ascii(data.file_name);
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
  const Phase phase = phaseOf(data.status);

  Ui::Canvas c = ui.frame(phase == PHASE_IDLE ? String("Printer") : jobTitle(data));
  if (phase == PHASE_IDLE) drawIdle(c, data);
  else drawJob(c, data, phase);
}
