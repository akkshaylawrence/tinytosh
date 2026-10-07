#include "Screens.h"

#include "images.h"

void Screens::drawCurrency(Ui::Surface& ui, const AppState& state, int page) {
  const Config& config = state.config;
  const CurrencyData& data = state.currencies[page];
  const int multiplier = config.currency_multipliers[page];
  if (!data.updated) {
    ui.alert(icon_error, "No Currency Data");
    return;
  }

  String base = data.base, target = data.target;
  base.toUpperCase();
  target.toUpperCase();

  const float rate = data.rate * multiplier;
  const int decimals = rate < 10.0 ? 3 : rate < 100.0 ? 2 : rate < 1000.0 ? 1 : 0;
  const String rateText = String(rate, decimals);

  Ui::Canvas c = ui.frame(base + " / " + target);
  const int cx = c.w / 2;
  Ui::Font font = c.largestFit(rateText, c.w, Ui::FONT_HUGE);
  c.textCentered(rateText, cx, 2 + (21 - c.textHeight(font)) / 2, font);
  c.textCentered(String(multiplier) + " " + base + " in " + target, cx, 27);
  c.dotted(0, 38, c.w);

  int nameW = c.w;
  if (data.date.length() > 0) {
    c.textRight(data.date, c.w, 43, Ui::FONT_SMALL);
    nameW -= c.textWidth(data.date, Ui::FONT_SMALL) + 6;
  }
  if (!config.currency_fn) return;
  for (const CurrencyOption& option : allCurrencies) {
    if (data.base.equalsIgnoreCase(option.code)) {
      c.textFit(option.name, 0, 43, nameW, Ui::FONT_SMALL);
      break;
    }
  }
}
