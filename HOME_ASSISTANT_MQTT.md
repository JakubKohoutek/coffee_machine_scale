# Home Assistant MQTT Setup for Coffee Scale

## 1. Install Mosquitto MQTT Broker

In Home Assistant:
1. Go to **Settings > Add-ons > Add-on Store**
2. Search for **Mosquitto broker** and install it
3. Start the add-on and enable **Start on boot**
4. Go to **Settings > People > Users** and create a dedicated user for MQTT (e.g., `mqtt_user`)

## 2. Configure credentials.h

Add the following to your Arduino `credentials.h` library (in `~/Documents/Arduino/libraries/credentials/credentials.h`):

```cpp
#define MQTT_SERVER "192.168.1.X"  // Your Home Assistant IP
#define MQTT_PORT   1883
#define MQTT_USER   "mqtt_user"
#define MQTT_PASS   "your_password"
```

## 3. MQTT Topics

The scale publishes to the following topics under `coffee_scale/`:

| Topic | Type | Description |
|-------|------|-------------|
| `coffee_scale/available` | Retained | `online` / `offline` (LWT) |
| `coffee_scale/singleDose` | Retained | Single dose target (milligrams) |
| `coffee_scale/doubleDose` | Retained | Double dose target (milligrams) |
| `coffee_scale/singleNakedOvershoot` | Retained | Single naked portafilter overshoot / post-flow weight (mg) |
| `coffee_scale/singleSpoutedOvershoot` | Retained | Single spouted portafilter overshoot / post-flow weight (mg) |
| `coffee_scale/doubleNakedOvershoot` | Retained | Double naked portafilter overshoot / post-flow weight (mg) |
| `coffee_scale/doubleSpoutedOvershoot` | Retained | Double spouted portafilter overshoot / post-flow weight (mg) |
| `coffee_scale/shotCount` | Retained | Total shot count |
| `coffee_scale/lastShotTime` | Retained | Last shot duration (milliseconds) |
| `coffee_scale/sleep` | Retained | `true` / `false` |
| `coffee_scale/rssi` | Retained | WiFi signal strength in dBm, e.g. `-65`; updated while WiFi is connected |
| `coffee_scale/event` | Not retained | `extraction_started` (plain string), `extraction_complete` / `extraction_stopped` (JSON: `{"event":"...","weight":36.1,"time":28.5,"dose":"double","reason":"target_reached"}`) |

The scale subscribes to `coffee_scale/command` for remote setting changes (same format as the web API).

RSSI is published on MQTT connection and with the normal 30-second state updates. Background state updates pause during extraction and scale measurement. The retained reading represents the last measurement; use the availability topic to mark the sensor unavailable when the device disconnects.

## 4. Home Assistant Configuration

Add to your `configuration.yaml`:

```yaml
mqtt:
  sensor:
    - name: "Coffee Scale WiFi Signal"
      state_topic: "coffee_scale/rssi"
      availability_topic: "coffee_scale/available"
      device_class: signal_strength
      state_class: measurement
      unit_of_measurement: "dBm"

    - name: "Coffee Scale Shot Count"
      state_topic: "coffee_scale/shotCount"
      icon: "mdi:coffee"

    - name: "Coffee Scale Last Shot Time"
      state_topic: "coffee_scale/lastShotTime"
      unit_of_measurement: "ms"
      icon: "mdi:timer"
      value_template: "{{ (value | float / 1000) | round(1) }}"

    - name: "Coffee Scale Single Dose"
      state_topic: "coffee_scale/singleDose"
      unit_of_measurement: "mg"
      icon: "mdi:weight-gram"
      value_template: "{{ (value | float / 1000) | round(1) }}"

    - name: "Coffee Scale Double Dose"
      state_topic: "coffee_scale/doubleDose"
      unit_of_measurement: "mg"
      icon: "mdi:weight-gram"
      value_template: "{{ (value | float / 1000) | round(1) }}"

    - name: "Coffee Scale Event"
      state_topic: "coffee_scale/event"
      icon: "mdi:coffee-maker"

  binary_sensor:
    - name: "Coffee Scale Online"
      state_topic: "coffee_scale/available"
      payload_on: "online"
      payload_off: "offline"
      device_class: "connectivity"

    - name: "Coffee Scale Sleep"
      state_topic: "coffee_scale/sleep"
      payload_on: "true"
      payload_off: "false"
      icon: "mdi:sleep"
```

## 5. Sending Commands from Home Assistant

You can change settings via MQTT publish. Example automation or developer tools:

```yaml
service: mqtt.publish
data:
  topic: "coffee_scale/command"
  payload: "setSingleDose:36000"
```

Available commands:
- `setSingleDose:<milligrams>`
- `setDoubleDose:<milligrams>`
- `setSingleNaked:<milligrams>`
- `setSingleSpouted:<milligrams>`
- `setDoubleNaked:<milligrams>`
- `setDoubleSpouted:<milligrams>`

## 6. Example Dashboard Card

```yaml
type: entities
title: Coffee Scale
entities:
  - entity: binary_sensor.coffee_scale_online
  - entity: sensor.coffee_scale_shot_count
  - entity: sensor.coffee_scale_last_shot_time
  - entity: sensor.coffee_scale_single_dose
  - entity: sensor.coffee_scale_double_dose
  - entity: sensor.coffee_scale_event
  - entity: binary_sensor.coffee_scale_sleep
```
