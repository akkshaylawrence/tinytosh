#!/bin/sh
# Flashes the firmware onto an attached Tinytosh and checks the paths a host talks to it over.
# Usage: tools/device-smoke.sh <serial-port> <ip> [--no-flash]
#
# Quit the PC App first: it holds the serial port, and the device answers one PC at a time.
# The saved settings survive the flash. A few are changed and put back.
set -e
PORT="$1"
IP="$2"
test -n "$PORT" && test -n "$IP" || { echo "Usage: $0 <serial-port> <ip> [--no-flash]"; exit 2; }
cd "$(dirname "$0")/.."

if [ "$3" != "--no-flash" ]; then
  arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3:PartitionScheme=huge_app --upload --port "$PORT" TinytoshESP32
  # The boot that follows an upload has twice failed to join WiFi and sat in its setup portal.
  # A plain restart has joined every time.
  sh tools/device-log.sh "$PORT" 1 --reset > /dev/null
fi

exec python3 - "$PORT" "$IP" "$3" <<'EOF'
import fcntl, gzip, json, os, struct, sys, termios, time, urllib.request

port, ip, flashed = sys.argv[1], sys.argv[2], sys.argv[3] != "--no-flash"

def call(path, body=None):
    data = None if body is None else json.dumps(body).encode()
    request = urllib.request.Request(f"http://{ip}{path}", data=data, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(request, timeout=180) as reply:
        raw = reply.read()
        return (gzip.decompress(raw) if reply.headers.get("Content-Encoding") == "gzip" else raw).decode()

def state():
    return json.loads(call("/update"))

def check(ok, what):
    print(("ok    " if ok else "FAIL  ") + what, flush=True)
    if not ok:
        sys.exit(1)

# Boot shows two 3 second cards, joins WiFi and runs a full sync before it serves anything.
deadline = time.time() + 300
booted = None
while booted is None and time.time() < deadline:
    try:
        booted = state()
    except Exception:
        time.sleep(2)
check(booted is not None, "device answers /update")
if flashed:
    check(booted["status"]["system"]["uptime_s"] < 300, "device restarted into the new firmware")
check("settings-form" in call("/"), "web panel is served")

before = booted["config"]["screens"]["interval_sec"]
changed = before + 1
call("/save", {"screens": {"interval_sec": changed}})
check(state()["config"]["screens"]["interval_sec"] == changed, "a saved setting reads back")
call("/save", {"screens": {"interval_sec": before}})
check(state()["config"]["screens"]["interval_sec"] == before, "the setting is put back")

# A save runs a full sync, so turning every data screen on fetches from every source. Flights
# are left out: an empty sky is a valid answer. One source may be missing, because these are
# other people's servers. With the Bluetooth mouse on there is too little memory left for HTTPS
# after boot and none of this can pass, so it is skipped.
sources = {"show_weather": "weather", "show_aqi": "aqi", "show_daylight": "daylight", "show_moon": "moon",
           "show_population": "population", "show_currency": "currency"}
if booted["config"]["hardware"]["presence_ble"]:
    print("skip  data sources: the Bluetooth mouse is on", flush=True)
else:
    shown = {key: booted["config"]["screens"][key] for key in sources}
    call("/save", {"screens": {key: 1 for key in sources}})
    status = state()["status"]
    call("/save", {"screens": shown})
    fetched = {name: name in status for name in sources.values()}
    fetched["weather"] = "temp" in status.get("weather", {})
    fetched["currency"] = bool(status.get("currency", {}).get("data"))
    missing = [name for name, ok in fetched.items() if not ok]
    check(len(missing) <= 1, f"data sources fetched (missing: {', '.join(missing) or 'none'})")

stats = {"pc_id": "smoke-test:1", "cpu_percent": 12.5, "mem_percent": 40, "disk_percent": 50, "net_down_kb": 3,
         "media_status": "stopped", "media_name": "", "media_author": "", "media_album": "",
         "presence": {"session": 1, "lease": False, "lease_ms": 0, "paused": True, "profile": 2,
                      "trigger_seq": 0, "reset_seq": 0}}
reply = json.loads(call("/pc-stats", stats))
check(reply.get("status") == "ok" and "link" in reply.get("presence", {}), "/pc-stats answers with the presence status")

fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
attrs = termios.tcgetattr(fd)
attrs[0], attrs[1], attrs[3] = 0, 0, 0
attrs[2] = (termios.CS8 | termios.CREAD | termios.CLOCAL) & ~termios.HUPCL
attrs[4] = attrs[5] = termios.B115200
termios.tcsetattr(fd, termios.TCSANOW, attrs)
# See tools/device-log.sh for why the control lines are lowered in this order.
fcntl.ioctl(fd, termios.TIOCMBIC, struct.pack("I", termios.TIOCM_RTS))
fcntl.ioctl(fd, termios.TIOCMBIC, struct.pack("I", termios.TIOCM_DTR))
os.write(fd, b"GET_UPDATE\n")
received, deadline = b"", time.time() + 10
while b"SYS_UPDATE:" not in received and time.time() < deadline:
    try:
        received += os.read(fd, 4096)
    except BlockingIOError:
        time.sleep(0.05)
os.close(fd)
check(b"SYS_UPDATE:" in received, "serial GET_UPDATE answers SYS_UPDATE")
print("Device smoke test passed.")
EOF
