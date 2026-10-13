#!/bin/sh
# Prints what an attached Tinytosh writes to its serial port.
# Usage: tools/device-log.sh <serial-port> [seconds] [--reset]
#
# The ESP32-C3 reads the port's DTR and RTS lines as its reset and boot pins. Opening a port
# raises both and closing it drops both, and the wrong order leaves the chip waiting in its
# bootloader with a dark screen. This lowers them in the safe order and leaves them alone on
# close. --reset restarts the device first, which also brings it back from that state.
set -e
PORT="$1"
test -n "$PORT" || { echo "Usage: $0 <serial-port> [seconds] [--reset]"; exit 2; }

exec python3 - "$PORT" "${2:-30}" "$3" <<'PY'
import fcntl, os, struct, sys, termios, time

port, seconds, reset = sys.argv[1], float(sys.argv[2]), sys.argv[3] == "--reset"
fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
attrs = termios.tcgetattr(fd)
attrs[0], attrs[1], attrs[3] = 0, 0, 0
attrs[2] = (termios.CS8 | termios.CREAD | termios.CLOCAL) & ~termios.HUPCL
attrs[4] = attrs[5] = termios.B115200
termios.tcsetattr(fd, termios.TCSANOW, attrs)

dtr, rts = struct.pack("I", termios.TIOCM_DTR), struct.pack("I", termios.TIOCM_RTS)
fcntl.ioctl(fd, termios.TIOCMBIC, rts)
fcntl.ioctl(fd, termios.TIOCMBIC, dtr)
if reset:
    fcntl.ioctl(fd, termios.TIOCMBIS, rts)
    time.sleep(0.2)
    fcntl.ioctl(fd, termios.TIOCMBIC, rts)

deadline = time.time() + seconds
while time.time() < deadline:
    try:
        sys.stdout.write(os.read(fd, 4096).decode(errors="replace"))
        sys.stdout.flush()
    except BlockingIOError:
        time.sleep(0.05)
PY
