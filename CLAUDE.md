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
arduino-cli upload --fqbn esp8266:esp8266:d1_mini --port 10.0.1.28 coffee_machine_scale.ino
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
- **`log.h/.cpp`** — Timestamped logging to LittleFS `/log.txt` (100KB max, auto-truncation) + Serial. Viewable at `http://<IP>/log`.
- **`mqtt.h/.cpp`** — MQTT client under `coffee_scale/` topic tree. Auto-reconnect, LWT, state/event publishing, command subscription.
- **`web.h`** — PROGMEM HTML/CSS/JS for the web settings interface.
- **`icons.h`** — XBM bitmap data for OLED icons.

### UI State Machine

The UI is driven by a `Screen` enum with these states: `MenuScreen`, `SettingsScreen`, `Timer`, `InfoScreen`, `ScaleScreen`, `PortafilterScreen`, `OvershootScreen`. Navigation is handled by `handleRotation()`, `handleButtonPush()`, and `handleLongButtonPush()` which switch on `currentScreen`.

### Extraction Flow

`startExtraction()` is the core routine: it tares the load cells, turns on the relay (pump), reads weight through a 5-element median filter (rejects vibration spikes with less lag than averaging), applies a correction for initial vibration (after `timeOfCorrection` seconds), and stops when `correctedWeight > weightLimit - overshoot`. After pump stops, waits 3 seconds for liquid to settle (skippable via button), then locks the final weight on the OLED display until the user presses the button. Shot profile (weight over time) is captured every 200ms and saved to LittleFS.

### Persistent Settings (EEPROM)

All values are stored as `unsigned long` at fixed addresses (weights in milligrams, times in milliseconds):
- Single/double dose target weights (addresses 0, 4)
- Shot statistics and last shot time (addresses 8, 12)
- Overshoot (post-flow weight) per portafilter type: naked vs spouted, single vs double (addresses 16–28)
- Magic byte for corruption detection (address 32)
- Correction time — initial vibration settling period in ms (address 36, default 1200ms)

### Web Interface & API

- **Web UI** at `http://<IP>/` — configure all EEPROM settings (dose targets, overshoot), view status and shot profile chart
- **`GET /api/status`** — JSON with all settings and device status
- **`GET /api/send?cmd=<command>`** — set values (e.g., `setSingleDose:36000`), with input validation
- **`GET /api/profile`** — JSON array of last shot profile (`[{"t":ms,"w":grams},...]`)
- **`GET /log`** — view log file
- Device auto-sleeps after 5 minutes of inactivity (display power save)

### MQTT

Topics under `coffee_scale/`: `available`, `singleDose`, `doubleDose`, `singleNakedOvershoot`, `singleSpoutedOvershoot`, `doubleNakedOvershoot`, `doubleSpoutedOvershoot`, `shotCount`, `lastShotTime`, `sleep`, `event`, `command`. The `event` topic publishes JSON with weight/time/dose/reason for extraction_complete and extraction_stopped events. See `HOME_ASSISTANT_MQTT.md` for HA integration guide.

## Libraries

ESP8266WiFi, ESPAsyncWebServer, PubSubClient, JC_Button, U8g2, HX711, LittleFS, ArduinoOTA, EEPROM
