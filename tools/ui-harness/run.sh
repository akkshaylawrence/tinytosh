#!/bin/sh
# Builds the UI library, every screen and the animated scenes against the real Adafruit_GFX on
# this machine, draws them in all four styles, and fails if any pixel lands outside 128x64.
# Sheets land in tools/ui-harness/out/.
# With --renders it also refreshes the setup guide's pictures in img/ (needs macOS sips).
set -e
cd "$(dirname "$0")"
FW=../../TinytoshESP32
GFX="${ADAFRUIT_GFX_DIR:-$(arduino-cli lib list --format json | python3 -c "
import json, sys
for entry in json.load(sys.stdin)['installed_libraries']:
    if entry['library']['name'] == 'Adafruit GFX Library': print(entry['library']['install_dir'])")}"
test -f "$GFX/Adafruit_GFX.cpp" || { echo "Adafruit GFX Library not found. Set ADAFRUIT_GFX_DIR."; exit 1; }
mkdir -p out/renders
g++ -std=c++17 -w -DARDUINO=10800 -DUI_LAYOUT_AUDIT -Ishim -I"$GFX" -I$FW \
  main.cpp "$GFX/Adafruit_GFX.cpp" $FW/Ui.cpp $FW/SaverScenes.cpp \
  $(ls $FW/Screen*.cpp | grep -v ScreenHost.cpp) \
  -o out/harness
./out/harness
if [ "$1" = "--renders" ]; then
  for bmp in out/renders/*.bmp; do
    sips -s format png "$bmp" --out "../../img/$(basename "${bmp%.bmp}").png" >/dev/null
  done
  echo "img/render_*.png refreshed."
fi
