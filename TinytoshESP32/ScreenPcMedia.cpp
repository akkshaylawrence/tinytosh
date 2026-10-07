#include "Screens.h"

#include "images.h"

bool Screens::mediaHasData(const AppState& state) {
  const PcMedia& media = state.media;
  return media.status.length() > 0 && media.name.length() > 0 && media.author.length() > 0 && !media.name.equalsIgnoreCase("Unknown");
}

void Screens::drawPcMedia(Ui::Surface& ui, const AppState& state, int) {
  if (!mediaHasData(state)) {
    ui.alert(icon_note, "No Media");
    return;
  }

  const PcMedia& media = state.media;
  String status = media.status;
  status.toUpperCase();
  const unsigned char* statusIcon = status == "PLAYING" ? icon_play : status == "PAUSED" ? icon_pause : icon_stop;
  const bool hasAlbum = media.album.length() > 0 && !media.album.equalsIgnoreCase("Unknown");

  Ui::Canvas c = ui.frame("Now Playing");
  c.icon(icon_note, 0, 0, 32, 32);
  {
    Ui::Group state(status.c_str());
    c.icon(statusIcon, 0, 38, 8, 8);
    c.textFit(status, 10, 40, 28, Ui::FONT_SMALL);
  }

  const int x = 42, textW = c.w - x;
  int y = 0;
  c.paragraph(Ui::ascii(media.name), x, y, textW, Ui::FONT_NORMAL, hasAlbum ? 2 : 3);
  y += 2;
  c.paragraph(Ui::ascii(media.author), x, y, textW, Ui::FONT_NORMAL, 1);
  if (!hasAlbum) return;
  y += 2;
  c.paragraph(Ui::ascii(media.album), x, y, textW, Ui::FONT_SMALL, 2);
}
