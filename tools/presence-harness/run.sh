#!/bin/sh
# Builds the firmware's gesture engine for this machine and runs it against a fake clock and a
# seeded random source. Checks control flow only: no radio, no HID delivery.
set -e
cd "$(dirname "$0")"
mkdir -p out
g++ -std=c++17 -Wall -Wextra -I../../TinytoshESP32 main.cpp ../../TinytoshESP32/PresenceEngine.cpp -o out/harness
./out/harness
