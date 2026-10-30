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

const int LABEL_Y = 0;
const int VALUE_Y = 7;
const int BAR_Y = 23;
const int BAR_H = 20;
const int FOOTER_Y = 45;

void drawLeftStat(Ui::Canvas& c, const String& label, const String& value) {
  c.text(label, 0, LABEL_Y, Ui::FONT_SMALL);
  c.text(value, 0, VALUE_Y, Ui::FONT_LARGE);
}

void drawRightStat(Ui::Canvas& c, const String& label, const String& value) {
  c.textRight(label, c.w, LABEL_Y, Ui::FONT_SMALL);
  c.textRight(value, c.w, VALUE_Y, Ui::FONT_LARGE);
}

void drawTemp(Ui::Canvas& c, const String& label, float degrees, bool alignRight) {
  const String value = String((int)round(degrees));
  const int x = alignRight ? c.w - c.textWidth(value, Ui::FONT_LARGE) - 5 : 0;
  if (alignRight) c.textRight(label, c.w, LABEL_Y, Ui::FONT_SMALL);
  else c.text(label, 0, LABEL_Y, Ui::FONT_SMALL);
  c.textWithMark(value, x, VALUE_Y, Ui::FONT_LARGE, degree_icon_small, 4, 4);
}

void drawProgress(Ui::Canvas& c, const String& label, int percent) {
  percent = percent < 0 ? 0 : percent > 100 ? 100 : percent;
  Ui::Group bar("progress bar");
  c.rect(0, BAR_Y, c.w, BAR_H);
  c.textCentered(label, c.w / 2, BAR_Y + 3, Ui::FONT_LARGE);
  // Fills past the digits' height, so inverted digits keep lit rows above and below them.
  c.invert(1, BAR_Y + 1, (c.w - 2) * percent / 100, BAR_H - 2);
}

void drawFans(Ui::Canvas& c, const BambuData& data) {
  c.textRight("F" + String(data.fan_part) + " A" + String(data.fan_aux), c.w, FOOTER_Y, Ui::FONT_SMALL);
}

void drawIdle(Ui::Canvas& c, const BambuData& data) {
  drawTemp(c, "Noz", data.nozzle_temp, false);
  drawTemp(c, "Bed", data.bed_temp, true);
  drawProgress(c, "IDLE", 0);
  drawFans(c, data);
}

void drawJob(Ui::Canvas& c, const BambuData& data, Phase phase) {
  if (phase == PHASE_FINISHED) drawLeftStat(c, "Print", "DONE");
  else if (phase == PHASE_FAILED) drawLeftStat(c, "Print", "FAILED");
  else drawLeftStat(c, "Left", formatTimeLeft(data.time_left));
  drawRightStat(c, "Progress", String(data.progress) + "%");

  const String layers = String(data.layer) + "/" + String(data.total_layers);
  drawProgress(c, phase == PHASE_PREPARING ? String("PREPARING") : phase == PHASE_PAUSED ? String("PAUSED") : layers, data.progress);

  c.text("N" + String((int)round(data.nozzle_temp)) + " B" + String((int)round(data.bed_temp)), 0, FOOTER_Y, Ui::FONT_SMALL);
  drawFans(c, data);
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
