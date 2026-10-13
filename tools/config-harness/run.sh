#!/bin/sh
# Builds the firmware's config serializer for this machine and checks the JSON it writes and
# reads against expected/. Covers the defaults, a file that sets every setting, a file saved by
# firmware old enough to list the Stock and Crypto screens, and a file that sets only a few.
# Fails too if fixtures/saved.json leaves a setting at its default, so a new setting needs a
# line there before this passes.
set -e
cd "$(dirname "$0")"
JSON="${ARDUINOJSON_DIR:-$(arduino-cli lib list --format json | python3 -c "
import json, sys
for entry in json.load(sys.stdin)['installed_libraries']:
    if entry['library']['name'] == 'ArduinoJson': print(entry['library']['install_dir'])")}"
test -f "$JSON/src/ArduinoJson.h" || { echo "ArduinoJson not found. Set ARDUINOJSON_DIR."; exit 1; }
mkdir -p out
g++ -std=c++17 -w -DARDUINO=10800 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 \
  -DARDUINOJSON_ENABLE_PROGMEM=0 -include host-doc.h -I../ui-harness/shim -I"$JSON/src" -I../../TinytoshESP32 \
  main.cpp ../../TinytoshESP32/JsonSerializer.cpp -o out/harness

./out/harness default > out/default.json
for name in saved legacy partial; do
  ./out/harness roundtrip fixtures/$name.json > out/$name.json
done

exec python3 - "$1" <<'PY'
import json, sys

def load(path):
    return json.load(open(path))

if sys.argv[1] == "--record":
    for name in ("default", "saved", "legacy", "partial"):
        open(f"expected/{name}.json", "w").write(json.dumps(load(f"out/{name}.json"), indent=2) + "\n")
    print("expected/ rewritten from this build.")
    sys.exit(0)

failed = False
for name in ("default", "saved", "legacy", "partial"):
    got, want = load(f"out/{name}.json"), load(f"expected/{name}.json")
    for group in sorted(set(got) | set(want)):
        for key in sorted(set(got.get(group, {})) | set(want.get(group, {}))):
            if got.get(group, {}).get(key) != want.get(group, {}).get(key):
                failed = True
                print(f"{name}: {group}.{key} is {got.get(group, {}).get(key)!r}, expected {want.get(group, {}).get(key)!r}")

default, saved = load("out/default.json"), load("out/saved.json")
for group, fields in default.items():
    for key, value in fields.items():
        if group != "network" and saved[group][key] == value:
            failed = True
            print(f"fixtures/saved.json leaves {group}.{key} at its default")

if failed:
    sys.exit(1)
print("Config serializer checks passed.")
PY
