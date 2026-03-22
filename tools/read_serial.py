#!/usr/bin/env python3
"""Read serial output from device, triggering a reset first."""
import serial
import time
import sys

PORT = "/dev/ttyACM0"
BAUD = 115200
READ_SECONDS = 20

s = serial.Serial(PORT, BAUD, timeout=1)
# Toggle DTR to reset the ESP32
s.setDTR(False)
time.sleep(0.1)
s.setDTR(True)
time.sleep(2)

out = b""
deadline = time.time() + READ_SECONDS
while time.time() < deadline:
    chunk = s.read(512)
    out += chunk
    if chunk:
        sys.stdout.write(chunk.decode(errors="replace"))
        sys.stdout.flush()
    time.sleep(0.1)

s.close()
