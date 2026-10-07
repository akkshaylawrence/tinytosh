#!/bin/sh
set -e
cd "$(dirname "$0")"
mkdir -p out
g++ -std=c++17 -Wall -I. -I../../TinytoshESP32 main.cpp ../../TinytoshESP32/SaverScenes.cpp -o out/harness
./out/harness
