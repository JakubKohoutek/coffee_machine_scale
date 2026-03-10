#!/bin/bash
set -e

# OTA upload script for ESP8266 Arduino projects via Home Assistant SSH relay.
# macOS blocks incoming espota connections due to the firewall, so this script
# compiles locally, copies the binary to HA over SSH, and runs espota.py there.
#
# Usage: upload.sh <device-mDNS-name-or-IP>
# Example: upload.sh "coffee-machine-scale"
#         upload.sh 192.168.68.64
#
# The script compiles the sketch in the current directory and uploads it to
# the device resolved via mDNS (or directly by IP). Requires the HA Terminal
# & SSH add-on.

ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
FQBN="esp8266:esp8266:d1_mini"
ESPOTA_PY=$(find ~/Library/Arduino15/packages/esp8266/hardware/esp8266/*/tools -name "espota.py" 2>/dev/null | head -1)

HA_HOST="homeassistant.local"
HA_USER="jakub"
HA_DIR="/home/jakub"

if [ -z "$1" ]; then
  echo "Usage: $(basename "$0") <device-mDNS-name-or-IP>"
  echo "Example: $(basename "$0") coffee-machine-scale"
  echo "         $(basename "$0") 192.168.68.64"
  exit 1
fi

if [ -z "$ESPOTA_PY" ]; then
  echo "Error: Could not find espota.py in Arduino ESP8266 package"
  exit 1
fi

DEVICE_NAME="$1"
SKETCH_DIR="$(pwd)"
INO_COUNT=$(find "$SKETCH_DIR" -maxdepth 1 -name "*.ino" | wc -l)
SKETCH_FILE=$(find "$SKETCH_DIR" -maxdepth 1 -name "*.ino" | head -1)

if [ -z "$SKETCH_FILE" ]; then
  echo "Error: No .ino file found in $SKETCH_DIR"
  exit 1
fi

if [ "$INO_COUNT" -gt 1 ]; then
  echo "Error: Multiple .ino files found in $SKETCH_DIR — expected exactly one"
  exit 1
fi

SKETCH_NAME=$(basename "$SKETCH_FILE")

echo "=== Compiling ${SKETCH_NAME} ==="
"$ARDUINO_CLI" compile --fqbn "$FQBN" --libraries ~/Documents/Arduino/libraries "$SKETCH_DIR"

BIN_NAME="${SKETCH_NAME}.bin"
BIN_FILE=$(find ~/Library/Caches/arduino/sketches -name "$BIN_NAME" -exec stat -f '%m %N' {} \; 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)
if [ -z "$BIN_FILE" ]; then
  echo "Error: Could not find compiled binary ($BIN_NAME)"
  exit 1
fi

echo ""
if echo "$DEVICE_NAME" | grep -qE '^([0-9]{1,3}\.){3}[0-9]{1,3}$'; then
  DEVICE_IP="$DEVICE_NAME"
  echo "=== Using IP ${DEVICE_IP} ==="
else
  echo "=== Resolving ${DEVICE_NAME} ==="
  TMPFILE=$(mktemp)
  trap 'rm -f "$TMPFILE"' EXIT

  dns-sd -G v4 "${DEVICE_NAME}.local" > "$TMPFILE" 2>&1 &
  DNS_PID=$!
  sleep 3
  kill "$DNS_PID" 2>/dev/null || true
  wait "$DNS_PID" 2>/dev/null || true

  DEVICE_IP=$(grep -oE '([0-9]{1,3}\.){3}[0-9]{1,3}' "$TMPFILE" | head -1)

  if [ -z "$DEVICE_IP" ]; then
    echo "Error: Could not resolve '${DEVICE_NAME}' via mDNS"
    exit 1
  fi
  echo "Found at ${DEVICE_IP}"
fi

echo ""
echo "=== Uploading OTA via ${HA_HOST} ==="
ssh "${HA_USER}@${HA_HOST}" "cat > '${HA_DIR}/firmware.bin'" < "$BIN_FILE"
ssh "${HA_USER}@${HA_HOST}" "cat > '${HA_DIR}/espota.py'" < "$ESPOTA_PY"
ssh "${HA_USER}@${HA_HOST}" "python3 '${HA_DIR}/espota.py' -i '${DEVICE_IP}' -p 8266 --auth= -f '${HA_DIR}/firmware.bin' -r"

echo ""
echo "=== Done ==="
