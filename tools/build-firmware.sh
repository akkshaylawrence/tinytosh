#!/bin/sh
# Builds the firmware and refreshes Tinytosh.bin, the 4 MB image the web installer flashes.
#
# The firmware is about 1.6 MB and the default ESP32 layout only gives an app 1.2 MB, because it
# reserves a second slot for over-the-air updates and a filesystem. Tinytosh uses neither (settings
# live in NVS), so it builds with the Huge APP layout: one 3 MB app slot.
set -e
cd "$(dirname "$0")/.."
OUT="${TMPDIR:-/tmp}/tinytosh-firmware"
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3:PartitionScheme=huge_app --output-dir "$OUT" TinytoshESP32
cp "$OUT/TinytoshESP32.ino.merged.bin" Tinytosh.bin
SIZE=$(wc -c < Tinytosh.bin | tr -d ' ')
test "$SIZE" = 4194304 || { echo "Tinytosh.bin is $SIZE bytes, expected 4194304."; exit 1; }
echo "Tinytosh.bin rebuilt ($SIZE bytes)."
