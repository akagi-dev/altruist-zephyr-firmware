# Altruist ESP32 Firmware Architecture (Source Baseline)

This document captures the current architecture of `airalab/altruist-firmware` on branch `esp32` as the source baseline for the Zephyr port.

## 1) System roles in Robonomics

The firmware has two independent reporting planes:

- **On-chain datalog** every ~10 minutes via Robonomics/Substrate RPC extrinsics.
- **Connectivity/map reporting** every ~30 seconds via signed HTTP POST to connectivity providers on port `65`.

Key source references:

- `airrohr-firmware.ino` (main scheduler + API dispatch)
- `apis/robonomics_datalog_api.cpp`
- `apis/robonomics_http_api.cpp`
- `apis/helpers/message_formatter.cpp`
- `robonomics_servers.h`

## 2) High-level component diagram

```mermaid
flowchart LR
    Sensors[Sensor drivers\nUART/I2C/I2S/HTTP] --> Agg[JSON aggregation\nDynamicJsonDocument sensors_data]
    Agg --> API1[RobonomicsDatalogAPI\n(sendRWSDatalogRecord)]
    Agg --> API2[RobonomicsHTTPAPI\nHTTP POST :65]
    Agg --> API3[CustomHTTPAPI]
    Agg --> UI[Web UI /status /values /config]
    Agg --> Insight[Insight display/analytics]

    CFG[SPIFFS /config.json\nconfig_manager] --> Sensors
    CFG --> API1
    CFG --> API2
    CFG --> UI

    ID[ED25519 keypair\nRobonomics private key] --> API1
    ID --> API2

    WIFI[WiFi STA/AP + captive portal\nImprov serial] --> UI
    WIFI --> API1
    WIFI --> API2
    WIFI --> OTA

    OTA[Two-stage OTA\nStable/testing channel metadata]
```

## 3) Runtime data flow

### 3.1 Sensor loop and aggregation

- Sensors implement a common base `Sensor` interface (`sensors/sensor.h`), with dynamic creation via `createSensor(...)` (`sensors/sensor_factory.h`).
- Sensor worker task (`sensorAndAPIWorker` in `airrohr-firmware.ino`) periodically calls `fetchSensors()` and updates shared JSON (`sensors_data`) under a mutex.
- Measurement payload is normalized as nested JSON with `{ value, intl_name, units }` fields.

### 3.2 API send path

- `API` base class (`apis/api.h`) tracks send intervals, timestamps, success counters, and health.
- `setupEnabledAPIs()` in `airrohr-firmware.ino` wires:
  - `RobonomicsDatalogAPI`
  - `RobonomicsHTTPAPI`
  - optional `CustomHTTPAPI`
- For each due API, sensor JSON is snapshotted and sent.

### 3.3 Signature and message shaping

- `apis/helpers/message_formatter.cpp` converts sensor JSON into short-field datalog strings (e.g. `p1`, `p2`, `t`, `co2`), respecting per-measurement sharing flags.
- `addTimeAndSign(...)` appends timestamp fragment and signs through Robonomics client (`Robonomics::signMessage`).

## 4) Identity and crypto lifecycle

- Private key is persisted in config as `private_key` (`config_manager/config_defaults.h`, `config_manager/airrohr-cfg.h`, `config_manager/config_helpers.cpp`).
- `RobonomicsDatalogAPI::setup()` (`apis/robonomics_datalog_api.cpp`):
  - if no key in config: generates keypair via Robonomics lib and stores it.
  - otherwise restores from config.
- Same identity signs both datalog and connectivity payloads.

## 5) Configuration lifecycle (SPIFFS)

- Config schema is centralized in generated `config_manager/airrohr-cfg.h` and defaults in `config_manager/config_defaults.h`.
- Persistence and migration logic live in `config_manager/config_helpers.cpp`:
  - read/write `/config.json`
  - migration for legacy fields
  - WiFi/web auth reset helpers
- Runtime reset semantics:
  - Urban GPIO7 hold at boot → full factory reset (including identity).
  - Runtime long hold on GPIO7 → WiFi + web credentials reset, identity kept.
  - Insight has button combinations for WiFi/full reset.
  - Implemented in `airrohr-firmware.ino`, `buttons/button_manager.cpp`, `wifi_manager.cpp`.

## 6) Connectivity and provisioning model

- WiFi manager (`wifi_manager.cpp`) provides:
  - STA mode operation
  - AP + captive portal config mode
  - STA runtime recovery and reconnect handling
  - mDNS service registration (`altruist._tcp`) except C3-lite builds.
- Improv serial provisioning protocol (`improv/improv_serial.cpp`) supports WiFi credentials and optional owner update.

## 7) OTA model

- OTA implementation: `OTA_Update.cpp`.
- Artifact naming/channel behavior controlled by build metadata (`platformio_build_metadata.py`) and channel flags.
- Partition layouts:
  - `esp32c6_partitions.csv` (dual app slots + SPIFFS)
  - `esp32c3_partitions.csv` (smaller dual app slots + SPIFFS)
- Automatic OTA respects channel policy and `cfg::auto_update`; manual OTA endpoint available in web UI (`webserver/webserver.cpp`).

## 8) Hardware variants

## Urban (ESP32-C6 and ESP32-C3 legacy)

- Primary outdoor sensor station build flags in `platformio.ini` (`ALTRUIST_URBAN`, C3-lite reductions).
- NeoPixel status LED behavior in `leds/leds_controller_urban.cpp`.
- C3-lite excludes heavier modules via `build_src_filter_esp32c3_urban` in `platformio.ini`.

## Insight (ESP32-C6)

- Indoor display-focused build (`ALTRUIST_INSIGHT` in `platformio.ini`).
- E-ink display and multi-screen UX in `display/display_manager.cpp` and screen modules.
- Aggregates Urban data over HTTP/mDNS via `sensors/http_altruist_sensor.cpp`.
- Button-driven navigation via `buttons/button_manager.cpp`.

## 9) Sensor subsystem inventory

From `sensors/sensor_factory.h`, `sensors/sensor_names.h`, and sensor driver files:

- SDS011 (PM2.5/PM10)
- BMx280 (temperature/humidity/pressure)
- BME680 (Insight)
- SCD4x (CO2/temperature/humidity)
- RadSens (radiation)
- I2S microphone noise sensor
- TinyGPS (Neo-6M)
- AGS3871
- ZMOD4510
- HTTP Altruist sensor (Insight ← Urban)
- DHT library present at repo root (`DHT.cpp`)

## 10) License/porting note

Source firmware is GPL-3.0 licensed (see project headers and repository license). Any direct code port/reuse into Zephyr implementation packages must preserve GPL-3.0 compatibility and attribution.
