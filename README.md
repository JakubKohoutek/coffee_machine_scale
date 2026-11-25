# Built-in Scale for Rancilio Silvia

Automatic espresso scale for the Rancilio Silvia. Measures extraction weight in real-time and stops the pump when the target weight is reached, accounting for post-flow overshoot.

**Board:** Wemos D1 Mini (ESP8266)

## Hardware

- 2x 1kg HX711 load cells (summed)
- SH1106 128x64 OLED display (I2C)
- Rotary encoder with push button
- Relay on D8 (solenoid valve + pump control)
- WiFi for OTA updates, web UI, and MQTT

## Circuit Diagram

![Circuit Diagram](./circuit_diagram/circuit_diagram.png)

## 3D Printed Parts

Load cell mounts: [Printables model](https://www.printables.com/model/897813-rancilio-silvia-loadcell-mount-slide-on-10mm-screw)

![Load cells](./images/load_cells.png)

Additional component housings in [housing_models/](./housing_models).

## Build & Upload

Requires `arduino-cli` on PATH (symlinked from Arduino IDE bundled binary):

```bash
# MQTT uses the asynchronous client (with the existing ESPAsyncTCP library)
arduino-cli lib install "AsyncMqttClient@0.9.0"

# Compile
arduino-cli compile --fqbn esp8266:esp8266:d1_mini --libraries ~/Documents/Arduino/libraries coffee_machine_scale.ino

# Upload (OTA)
arduino-cli upload --fqbn esp8266:esp8266:d1_mini --port <DEVICE_IP> coffee_machine_scale.ino
```

WiFi and MQTT credentials go in `~/Documents/Arduino/libraries/credentials/credentials.h` — see `HOME_ASSISTANT_MQTT.md`.

Device web UI: http://coffee-machine-scale.local/ (currently http://192.168.68.71/).

## Features

- **Auto-stop extraction** — stops pump at target weight minus configurable overshoot (post-flow compensation)
- **Correction time** — configurable initial settling period (default 1.2s) to ignore vibration after pump starts
- **Settling delay** — waits 3s after pump stops for remaining liquid, then locks final weight on display
- **Shot profile** — records weight every 200ms; viewable as a chart in the web UI
- **Dual portafilter support** — separate overshoot settings for naked/spouted, single/double
- **Web UI** at `http://<IP>/` — configure dose targets, overshoot, view shot profile and logs
- **MQTT** — publishes state under `coffee_scale/`, extraction events as JSON (weight, time, dose type, stop reason)
- **OTA updates** — flash new firmware over WiFi
- **Offline-capable** — WiFi association/retries and MQTT connections run in the background without blocking menu input or startup
- **Responsive shot results** — networking stays serviced while the final weight is displayed, avoiding overdue reconnect work when returning to the menu
- **Diagnostics** — persistent boot/reset details, uptime, WiFi disconnect reasons, MQTT connection timing, memory health, and slow-operation reports
- **Watchdog** — hardware WDT resets device if firmware locks up during extraction (relay fails safe to LOW)
- **EEPROM validation** — magic byte detects uninitialized memory and writes sensible defaults

## Web API

| Endpoint | Description |
|----------|-------------|
| `GET /` | Web settings UI |
| `GET /api/status` | JSON with all settings and device state |
| `GET /api/send?cmd=<command>` | Set values (e.g. `setSingleDose:36000`) |
| `GET /api/profile` | Last shot profile as JSON array |
| `GET /log` | Timestamped log file |
| `GET /log/previous` | Previous rotated log file (404 until the first rotation) |

### Diagnosing freezes

Save `/log` and `/log/previous` after an incident; logs survive restarts. `[BOOT]` records reset details and startup progress. Each entry includes `up_ms`, even when NTP is unavailable (`clock-unsynced`). `[WIFI]` records disconnect reason codes and retry requests; `[MQTT]` records asynchronous connection outcomes and retry delays. `[HEALTH]` reports heap, largest free block, fragmentation, connectivity, screen, and maximum observed operation/log-write times every minute while the menu or completed-shot screen is active. `[SLOW]` identifies background operations or profile saves taking at least 250ms (rate-limited to one report per 30s).

Logging rotates at approximately 100KB, keeping the previous file instead of copying its contents in the input-processing thread. A failed filesystem mount is not automatically formatted, to preserve diagnostic evidence. Background service work is not run inside the pump/settling loops; normal asynchronous network callbacks still run when the firmware yields.

## MQTT Topics

All under `coffee_scale/`. See [HOME_ASSISTANT_MQTT.md](./HOME_ASSISTANT_MQTT.md) for full integration guide.

## UI Navigation

- Main menu shows a WiFi icon (crossed out while disconnected); Info shows the full IPv4 address or `IP: offline`. Both refresh when connectivity changes.
- **Rotate** — navigate menus, adjust values
- **Press** — select / confirm
- **Long press** — edit dose target (from menu) or overshoot (from portafilter menu)
- Auto-sleep after 5 minutes; any input wakes the display
