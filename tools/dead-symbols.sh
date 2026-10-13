#!/bin/sh
# Lists firmware symbols that nothing refers to: functions and constants a header declares, and
# bitmaps in images.h. A function counts as used when its name appears more than twice (the
# declaration and the definition), a constant or bitmap when it appears more than once.
# It counts names, not calls, so a hit is a prompt to look and not a proof.
set -e
cd "$(dirname "$0")/../TinytoshESP32"

ALL=$(mktemp)
trap 'rm -f "$ALL"' EXIT
cat *.h *.cpp *.ino > "$ALL"
HEADERS=$(ls *.h | grep -v images.h)
# Overrides the BLE and port base classes call, and words the function pattern picks up from data.
IGNORE='^(onConnect|onDisconnect|Congo)$'

uses() { grep -ow "$1" "$ALL" | wc -l | tr -d ' '; }
report() {
  while read -r name; do
    [ "$(uses "$name")" -le "$1" ] && echo "$2 $name"
  done
}

FOUND=$({
  sed 's|//.*||' $HEADERS | grep -oE '\b[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\(' | tr -d ' (' | sort -u | grep -vE "$IGNORE" | report 2 function
  grep -hoE '\b(const|constexpr)[^=;(]*\b[A-Za-z_][A-Za-z0-9_]*[[:space:]]*(=|\[)' $HEADERS \
    | grep -oE '[A-Za-z_][A-Za-z0-9_]*[[:space:]]*(=|\[)$' | tr -d ' =[' | sort -u | report 1 constant
  grep -oE '\b(icon|img|bmp|image|logo|frame|sprite)[A-Za-z0-9_]*\b' images.h | sort -u | report 1 bitmap
} || true)

if [ -n "$FOUND" ]; then
  echo "$FOUND"
  echo "Unused firmware symbols found."
  exit 1
fi
echo "No unused firmware symbols."
