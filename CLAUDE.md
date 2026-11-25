# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Arduino firmware for a built-in espresso scale for the Rancilio Silvia coffee machine. It measures extracted coffee weight and automatically stops extraction when a target weight is reached. The board is a **Wemos D1 Mini (ESP8266)**.

## Build & Upload

This is an Arduino project. Use `arduino-cli` for compilation and upload:

`arduino-cli` is available system-wide via a symlink (`/usr/local/bin/arduino-cli` → Arduino IDE bundled binary).

```bash
# Compile
arduino-cli compile --fqbn esp8266:esp8266:d1_mini --libraries ~/Documents/Arduino/libraries coffee_machine_scale.ino

# Upload over the air (OTA) — device IP is the "port"
arduino-cli upload --fqbn esp8266:esp8266:d1_mini --port 192.168.68.71 coffee_machine_scale.ino
```

The board configuration string from `.vscode/arduino.json`:
`xtal=80,vt=flash,exception=disabled,stacksmash=disabled,ssl=all,mmu=3232,non32xfer=fast,eesz=4M2M,ip=lm2f,dbg=Disabled,lvl=None____,wipe=none,baud=921600`

## Architecture

### Hardware

- Two 1kg **HX711 load cells** (loadcellA on D3/D4, loadcellB on TX/RX) — combined weight is summed
- **SH1106 128x64 OLED** display via I2C (U8g2 library)
- **Rotary encoder** with push button (pins 14/12/13) for UI navigation
- **Relay** on D8 controls the espresso machine's solenoid valve and pump
- WiFi credentials come from a `<credentials.h>` library (defines `STASSID`, `STAPSK`, and MQTT credentials)

### Software Modules

- **`coffee_machine_scale.ino`** — Main sketch: UI state machine, extraction logic, display rendering, web server, API endpoints, WiFi resilience.
- **`memory.h/.cpp`** — EEPROM persistence using a union-based byte serialization for `unsigned long` values. Addresses are defined as `#define` constants in the main sketch.
- **`rotary_encoder.h/.cpp`** — Interrupt-driven rotary encoder reading.
- **`ota.h/.cpp`** — ArduinoOTA wrapper for over-the-air firmware updates.
- **`log.h/.cpp`** — Wall-time and uptime logging to LittleFS `/log.txt` + Serial. Rotates at 100KB by rename to `/log_previous.txt`; available at `/log` and `/log/previous`. Does not auto-format after mount failure.
- **`mqtt.h/.cpp`** — AsyncMqttClient under `coffee_scale/` topic tree. Nonblocking connect/retry, LWT, state/event publishing, command subscription. TCP callbacks only queue commands/events; main-loop processing performs logging and settings writes.
- **`web.h`** — PROGMEM HTML/CSS/JS for the web settings interface.
- **`icons.h`** — XBM bitmap data for OLED icons.

### UI State Machine

The UI is driven by a `Screen` enum with these states: `MenuScreen`, `SettingsScreen`, `Timer`, `InfoScreen`, `ScaleScreen`, `PortafilterScreen`, `OvershootScreen`. Navigation is handled by `handleRotation()`, `handleButtonPush()`, and `handleLongButtonPush()` which switch on `currentScreen`.

### Extraction Flow

`startExtraction()` is the core routine: it tares the load cells, turns on the relay (pump), reads weight through a 5-element median filter (rejects vibration spikes with less lag than averaging), applies a correction for initial vibration (after `timeOfCorrection` seconds), and stops when `correctedWeight > weightLimit - overshoot`. After pump stops, waits 3 seconds for liquid to settle (skippable via button), then computes flow stats (`computeFlowStats()`), updates overshoot tracking (`updateOvershootAverage()`), saves the shot profile and history, and locks the final weight on the OLED display until the user presses the button. Shot profile (weight over time) is captured every 200ms and saved to LittleFS.

#### Flow Rate Stats (`ShotStats` struct)
After each extraction, `computeFlowStats()` iterates the shot profile to compute:
- **Average flow rate** (g/s) — final weight / (total time - correction time)
- **Peak flow rate** (g/s) — max of 5-point centered moving average of inter-point rates
- **Time to first drip** (seconds) — first profile point where weight > 0.5g
- **Weight at pump stop** — captured at relay off, used for overshoot tracking

#### Overshoot Auto-Learning
`updateOvershootAverage()` tracks actual vs configured overshoot per portafilter type. After each target-reached extraction, actual overshoot (settled weight - weight at pump stop) is fed into an EMA: true running average for the first 10 shots, then alpha=0.1. Values are persisted in EEPROM and displayed in the web UI below each overshoot setting.

#### Shot History
`addShotToHistory()` stores the last 20 shot summaries in `/shot_history.json` (LittleFS). Each entry includes shot number, weight, time, flow stats, dose type, portafilter, configured/actual overshoot, and stop reason. Exposed via `/api/history` and displayed in a table in the web UI.

`handleBackground()` services WiFi, MQTT, OTA, and health diagnostics from the main loop (after physical input) and the locked result screen, but not inside pump/settling loops. WiFi startup and retries never wait for association; OTA initializes after the first connection. Logs include boot/reset details, WiFi disconnect reason codes, MQTT timings, and rate-limited slow-operation reports.

### Persistent Settings (EEPROM)

All values are stored as `unsigned long` at fixed addresses (weights in milligrams, times in milliseconds):
- Single/double dose target weights (addresses 0, 4)
- Shot statistics and last shot time (addresses 8, 12)
- Overshoot (post-flow weight) per portafilter type: naked vs spouted, single vs double (addresses 16–28)
- Magic byte for corruption detection (address 32)
- Correction time — initial vibration settling period in ms (address 36, default 1200ms)
- Actual overshoot averages in mg per portafilter type (addresses 40–52)
- Sample counts for overshoot averaging (addresses 56–68)
- EEPROM size: 80 bytes

### Web Interface & API

- **Web UI** at `http://<IP>/` — warm copper/espresso-themed instrument panel UI. Configure all EEPROM settings (dose targets, overshoot), view status and shot profile chart with oscilloscope-style grid. Flow rate curve uses 5-point centered moving average for smooth display (raw rates shown in hover tooltips).
- **`GET /api/status`** — JSON with all settings and device status
- **`GET /api/send?cmd=<command>`** — set values (e.g., `setSingleDose:36000`), with input validation
- **`GET /api/profile`** — JSON with stats and profile: `{"stats":{"avgFlow","peakFlow","firstDrip"},"profile":[{"t":ms,"w":grams},...]}`
- **`GET /api/history`** — JSON array of last 20 shot summaries
- **`GET /log`** — view log file
- **`GET /log/previous`** — previous rotated log file
- Device auto-sleeps after 5 minutes of inactivity (display power save)

### MQTT

Topics under `coffee_scale/`: `available`, `singleDose`, `doubleDose`, `singleNakedOvershoot`, `singleSpoutedOvershoot`, `doubleNakedOvershoot`, `doubleSpoutedOvershoot`, `shotCount`, `lastShotTime`, `lastShot`, `sleep`, `event`, `command`. The `event` topic publishes JSON with weight/time/dose/reason/avgFlow/peakFlow/firstDrip for extraction_complete and extraction_stopped events. The `lastShot` topic (retained) publishes a compact JSON summary after each shot. See `HOME_ASSISTANT_MQTT.md` for HA integration guide.

## Libraries

ESP8266WiFi, ESPAsyncWebServer, ESPAsyncTCP, AsyncMqttClient (0.9.0), JC_Button, U8g2, HX711, LittleFS, ArduinoOTA, EEPROM
