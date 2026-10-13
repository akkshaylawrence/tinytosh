#!/bin/sh
# Loads a running Tinytosh's web server while its background sync runs, and fails if the device
# reboots. Usage: tools/device-stress.sh <ip> [minutes]
#
# It reads the page and the state as fast as the device answers. A reboot shows as uptime going
# backwards. The lowest free heap the device has seen is printed each minute, so a leak shows as
# a floor that keeps sinking. Give a screen a 1 minute custom sync first, or little will overlap.
set -e
IP="$1"
MINUTES="${2:-15}"
test -n "$IP" || { echo "Usage: $0 <ip> [minutes]"; exit 2; }

exec python3 - "$IP" "$MINUTES" <<'EOF'
import json, sys, time, urllib.request

ip, minutes = sys.argv[1], float(sys.argv[2])

def get(path):
    with urllib.request.urlopen(f"http://{ip}{path}", timeout=15) as reply:
        return reply.read()

deadline = time.time() + minutes * 60
next_report = time.time() + 60
last_uptime, requests, failures, reboots = None, 0, 0, 0
while time.time() < deadline:
    try:
        get("/")
        system = json.loads(get("/update"))["status"]["system"]
        requests += 2
    except Exception as error:
        failures += 1
        print(f"request failed: {error}", flush=True)
        time.sleep(1)
        continue
    if last_uptime is not None and system["uptime_s"] < last_uptime:
        reboots += 1
        print(f"REBOOT: uptime fell from {last_uptime}s to {system['uptime_s']}s", flush=True)
    last_uptime = system["uptime_s"]
    if time.time() >= next_report:
        next_report += 60
        print(f"uptime {last_uptime}s, free heap {system['free_heap']}, "
              f"heap floor {system['min_free_heap']}, {requests} requests, {failures} failed", flush=True)

print(f"{requests} requests, {failures} failed, {reboots} reboots in {minutes:g} min")
sys.exit(1 if reboots else 0)
EOF
