#!/bin/sh
# The simulator runs the firmware's own UI library and screens. Wokwi wants them as flat files
# in its project folder, so this copies them over. Run it after changing any of them.
# structs.h and images.h are not copied: the simulator keeps its own, with mock data.
set -e
cd "$(dirname "$0")/.."
for file in Ui.h Ui.cpp Screens.h Screens.cpp ScreenHost.h SaverScenes.h SaverScenes.cpp \
    $(cd TinytoshESP32 && ls Screen*.cpp | grep -v -e '^Screens.cpp$' -e '^ScreenHost.cpp$'); do
  cp "TinytoshESP32/$file" "TinytoshSimulator/$file"
done
echo "Simulator screens match the firmware."
