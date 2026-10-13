#!/bin/sh
# Everything that can be proven without a device: the PC app's lints and tests, the firmware's
# host harnesses, unused firmware symbols, and a full firmware compile.
# Pass --no-firmware to skip the compile, which takes a few minutes.
set -e
cd "$(dirname "$0")/.."

(cd TinytoshPC/src-tauri && cargo clippy --all-targets -- -D warnings && cargo test)
sh tools/ui-harness/run.sh
sh tools/presence-harness/run.sh
sh tools/config-harness/run.sh
sh tools/dead-symbols.sh

if [ "$1" != "--no-firmware" ]; then
  OUT="${TMPDIR:-/tmp}/tinytosh-check"
  arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3:PartitionScheme=huge_app --output-dir "$OUT" TinytoshESP32
  echo "Firmware app is $(wc -c < "$OUT/TinytoshESP32.ino.bin" | tr -d ' ') bytes."
fi
echo "All checks passed."
