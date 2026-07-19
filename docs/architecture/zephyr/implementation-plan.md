# Zephyr Port Master Implementation Plan

This plan splits work into independent packages sized for one Copilot coding-agent session each.

## Conventions

- **In scope / Out of scope** listed per package.
- **Dependencies** are strict prerequisites.
- **Tests** prefer Twister (`native_sim` for pure logic, board/HIL where hardware is required).
- Source baseline references are from `airalab/altruist-firmware` branch `esp32`.

---

## Phase 0 — Scaffolding

### WP-01 — West workspace and Zephyr app bootstrap
- **Goal:** Create buildable Zephyr app skeleton.
- **In scope:** `west.yml`, `app/` CMake/Kconfig/prj, hello/blinky smoke app.
- **Out:** Product logic.
- **Files:** `west.yml`, `app/CMakeLists.txt`, `app/prj.conf`, `app/src/main.c`.
- **Deps:** none.
- **Acceptance:** `west build` succeeds for `esp32c6_devkitc`; app boots.
- **Tests:** Twister smoke on `native_sim`; board build CI.

### WP-02 — CI foundation (Zephyr SDK + Twister)
- **Goal:** Add CI for lint/build/test on docs and code skeleton.
- **In scope:** workflow with Zephyr SDK toolchain setup and Twister subset.
- **Out:** Hardware-in-loop.
- **Files:** `.github/workflows/ci.yml` (or repo CI equivalent), `tests/smoke/*`.
- **Deps:** WP-01.
- **Acceptance:** CI runs formatting/build/twister subset and reports artifacts.
- **Tests:** Twister `native_sim` + compile-only board matrix.

### WP-03 — Board/variant configuration baseline
- **Goal:** Introduce Urban C3/C6 and Insight C6 board configs/Kconfig profiles.
- **In scope:** board overlays, Kconfig options, devicetree aliases.
- **Out:** full driver implementation.
- **Files:** `app/boards/*`, `app/Kconfig.variants`, `app/prj_*.conf`.
- **Deps:** WP-01.
- **Acceptance:** each profile compiles with distinct feature toggles.
- **Tests:** compile matrix for all variants.

---

## Phase 1 — Core platform

### WP-04 — Settings/config subsystem
- **Goal:** Port config lifecycle from SPIFFS JSON to Zephyr Settings.
- **In scope:** schema keys, load/save defaults, migration hooks.
- **Out:** full web UI forms.
- **Files:** `app/src/config/*`, `app/include/altruist/config.h`.
- **Deps:** WP-03.
- **Source refs:** `config_manager/config_helpers.cpp`, `config_manager/airrohr-cfg.h`.
- **Acceptance:** config persists reboot and supports reset operations.
- **Tests:** unit tests on settings encode/decode (`native_sim`).

### WP-05 — WiFi manager + provisioning baseline
- **Goal:** Implement STA management, reconnect policy, and provisioning entrypoints.
- **In scope:** `net_mgmt` events, reconnect backoff, AP/provisioning mode hooks.
- **Out:** full HTML UI.
- **Files:** `app/src/net/wifi_manager.c`, `app/src/provisioning/*`.
- **Deps:** WP-04.
- **Source refs:** `wifi_manager.cpp`, `improv/improv_serial.cpp`.
- **Acceptance:** reliable connect/disconnect recovery and provisioning state transitions.
- **Tests:** mocked net event tests (`native_sim`) + board compile checks.

### WP-06 — Identity/keystore + ED25519 signing
- **Goal:** Generate/persist keypair and provide signing API used by both report paths.
- **In scope:** key generation on first boot, secure storage, sign/verify helper API.
- **Out:** full Substrate extrinsic sender.
- **Files:** `app/src/crypto/*`, `app/include/altruist/identity.h`.
- **Deps:** WP-04.
- **Source refs:** `apis/robonomics_datalog_api.cpp`, `config_manager/config_helpers.cpp`.
- **Acceptance:** deterministic signing API, key survives reboot; reset behavior documented.
- **Tests:** vector-based sign/verify tests (`native_sim`).

---

## Phase 2 — Sensor framework and individual sensor packages

### WP-07 — Sensor framework core
- **Goal:** Build common sensor interface, scheduler, and snapshot data model.
- **In scope:** sensor registry/factory, polling cadence, shared payload model.
- **Out:** individual hardware drivers.
- **Files:** `app/src/sensors/core/*`, `app/include/altruist/sensors.h`.
- **Deps:** WP-03, WP-04.
- **Source refs:** `sensors/sensor.h`, `sensors/sensor_factory.h`, `airrohr-firmware.ino`.
- **Acceptance:** virtual/test sensors can register and publish measurements.
- **Tests:** framework unit tests (`native_sim`).

### WP-08 — SDS011 driver package
- **Goal:** Port SDS011 UART PM driver.
- **In scope:** UART protocol parse, PM2.5/PM10 output.
- **Out:** unrelated sensors.
- **Files:** `app/src/sensors/sds011/*`.
- **Deps:** WP-07.
- **Source refs:** `sensors/sds011_sensor.cpp`.
- **Acceptance:** produces PM metrics in standard snapshot schema.
- **Tests:** parser unit tests (`native_sim`) + compile on ESP targets.

### WP-09 — BMx280 driver package
- **Goal:** Port BMx280 temperature/humidity/pressure sensor support.
- **In scope:** I2C init/read + schema mapping.
- **Out:** BME680/SCD4x.
- **Files:** `app/src/sensors/bmx280/*`.
- **Deps:** WP-07.
- **Source refs:** `sensors/bmx280i2c_sensor.cpp`.
- **Acceptance:** stable periodic readings with units mapping.
- **Tests:** mocked sensor backend tests (`native_sim`).

### WP-10 — BME680 driver package
- **Goal:** Port BME680 support (Insight-focused).
- **In scope:** gas + environmental values.
- **Out:** analytics logic.
- **Files:** `app/src/sensors/bme680/*`.
- **Deps:** WP-07.
- **Source refs:** `sensors/bmx680i2c_sensor.cpp`.
- **Acceptance:** values map correctly and feature-gated by Kconfig.
- **Tests:** compile + unit conversion tests.

### WP-11 — SCD4x driver package
- **Goal:** Port SCD40/41 CO2 sensor support.
- **In scope:** CO2/temp/humidity polling and timeout handling.
- **Out:** cross-sensor blending policy.
- **Files:** `app/src/sensors/scd4x/*`.
- **Deps:** WP-07.
- **Source refs:** `sensors/scd4x_sensor.cpp`.
- **Acceptance:** valid CO2/temp/humidity values in snapshot model.
- **Tests:** driver logic unit tests.

### WP-12 — RadSens driver package
- **Goal:** Port radiation sensor support.
- **In scope:** I2C read and CPM reporting.
- **Out:** AQI logic.
- **Files:** `app/src/sensors/radsens/*`.
- **Deps:** WP-07.
- **Source refs:** `sensors/radsens_sensor.cpp`.
- **Acceptance:** CPM exported and feature-gated.
- **Tests:** compile + basic mocked read tests.

### WP-13 — I2S noise sensor package
- **Goal:** Port I2S microphone dBA measurement path.
- **In scope:** I2S capture pipeline + max/avg calculation.
- **Out:** display graphs.
- **Files:** `app/src/sensors/noise_i2s/*`.
- **Deps:** WP-07.
- **Source refs:** `sensors/i2s_noise_sensor.cpp`.
- **Acceptance:** noiseAvg/noiseMax exported with bounded CPU usage.
- **Tests:** algorithm unit tests on sample buffers (`native_sim`).

### WP-14 — GPS (Neo-6M) package
- **Goal:** Port UART GPS ingestion and coordinate reporting.
- **In scope:** NMEA parser integration + lat/lon fields.
- **Out:** map URL rendering.
- **Files:** `app/src/sensors/gps/*`.
- **Deps:** WP-07.
- **Source refs:** `sensors/tiny_gps_sensor.cpp`.
- **Acceptance:** valid coordinates when NMEA stream provided.
- **Tests:** parser replay tests (`native_sim`).

### WP-15 — AGS3871 package
- **Goal:** Port AGS3871 support.
- **In scope:** driver integration and data mapping.
- **Out:** policy/UI logic.
- **Files:** `app/src/sensors/ags3871/*`.
- **Deps:** WP-07.
- **Source refs:** `sensors/ags3871_sensor.cpp`.
- **Acceptance:** sensor compiles and reports expected fields.
- **Tests:** compile + mocked transaction tests.

### WP-16 — ZMOD4510 package
- **Goal:** Port ZMOD4510 support (NO2/O3/AQI fields).
- **In scope:** driver wrapper and schema mapping.
- **Out:** non-ZMOD sensors.
- **Files:** `app/src/sensors/zmod4510/*`.
- **Deps:** WP-07.
- **Source refs:** `sensors/zmod4510_sensor.cpp`, `sensors/drivers/zmod4510/*`.
- **Acceptance:** o3/no2/aqi fields available under feature flag.
- **Tests:** compile + mapping tests.

---

## Phase 3 — Networking APIs

### WP-17 — Connectivity HTTP POST API + server pool selection
- **Goal:** Implement signed map reporting every 30s with pool selection semantics.
- **In scope:** host pool parsing, least-load/registered selection, POST :65.
- **Out:** on-chain extrinsics.
- **Files:** `app/src/api/connectivity/*`.
- **Deps:** WP-05, WP-06, WP-07.
- **Source refs:** `apis/robonomics_http_api.cpp`, `robonomics_servers.h`.
- **Acceptance:** retry/selection behavior mirrors source logic.
- **Tests:** `native_sim` tests for parsing/selection + mocked HTTP.

### WP-18 — Substrate datalog API + SCALE/extrinsic module
- **Goal:** Implement 10-minute datalog extrinsic pipeline.
- **In scope:** SCALE codec module, extrinsic builder, RPC submission wrapper.
- **Out:** UI management.
- **Files:** `app/src/substrate/*`, `app/src/api/datalog/*`.
- **Deps:** WP-06, WP-07.
- **Source refs:** `apis/robonomics_datalog_api.cpp`, `apis/helpers/message_formatter.cpp`.
- **Acceptance:** signed payload created and submission path integrated.
- **Tests:** vector tests for SCALE encoding (`native_sim`).

### WP-19 — mDNS + HTTP Altruist aggregation sensor
- **Goal:** Port Insight discovery of nearby Urban devices and local HTTP aggregation.
- **In scope:** mDNS query/announce + Urban data ingestion.
- **Out:** display rendering.
- **Files:** `app/src/net/mdns_*`, `app/src/sensors/http_altruist/*`.
- **Deps:** WP-05, WP-07.
- **Source refs:** `wifi_manager.cpp`, `sensors/http_altruist_sensor.cpp`, `webserver/webserver.cpp`.
- **Acceptance:** Insight can discover/fallback-to-configured Urban endpoint and ingest data.
- **Tests:** mocked resolver + HTTP payload mapping tests.

---

## Phase 4 — Web configuration UI and i18n

### WP-20 — Configuration HTTP API and pages
- **Goal:** Port config/status endpoints and web pages.
- **In scope:** `/config`, `/status`, `/values`, `/ota` and related handlers.
- **Out:** complete Insight screen management.
- **Files:** `app/src/web/*`, `app/web_assets/*`.
- **Deps:** WP-04, WP-05.
- **Source refs:** `webserver/webserver.cpp`, `webserver/pages/*`, `webserver/html-content.h`.
- **Acceptance:** config can be viewed/updated from browser and persisted.
- **Tests:** handler unit tests (`native_sim`) where possible.

### WP-21 — i18n baseline (EN/RU)
- **Goal:** Port EN/RU translation pipeline used by web and Insight text.
- **In scope:** string tables, locale selection, fallback behavior.
- **Out:** additional languages beyond EN/RU.
- **Files:** `app/src/i18n/*`, `app/include/altruist/i18n.h`.
- **Deps:** WP-20.
- **Source refs:** `intl.h`, `translations/intl_en.h`, `translations/intl_ru.h`.
- **Acceptance:** EN/RU switch reflected in served UI and runtime labels.
- **Tests:** locale selection unit tests.

---

## Phase 5 — OTA, LEDs/buttons, SD card

### WP-22 — OTA/DFU with MCUboot and channel policy
- **Goal:** Replace Arduino OTA with Zephyr DFU/MCUboot two-slot pipeline.
- **In scope:** slot layout config, update check/download/install, stable/testing policy.
- **Out:** web UX polish.
- **Files:** `app/src/ota/*`, partition overlays.
- **Deps:** WP-03, WP-05.
- **Source refs:** `OTA_Update.cpp`, `esp32c3_partitions.csv`, `esp32c6_partitions.csv`, `platformio_build_metadata.py`.
- **Acceptance:** image update + rollback behavior validated.
- **Tests:** DFU simulation tests + board compile checks.

### WP-23 — LEDs/buttons/reset semantics
- **Goal:** Port Urban/Insight button events and LED state machine.
- **In scope:** RGB status policy, reset combinations, debounce/event model.
- **Out:** advanced display analytics.
- **Files:** `app/src/ui/buttons/*`, `app/src/ui/leds/*`.
- **Deps:** WP-05.
- **Source refs:** `leds/leds_controller_urban.cpp`, `buttons/button_manager.cpp`, `airrohr-firmware.ino`.
- **Acceptance:** behavior matches documented semantics for setup/healthy/error/reset.
- **Tests:** state-machine unit tests (`native_sim`).

### WP-24 — SD card logging and retention
- **Goal:** Port CSV logger and retention/rollup background tasks.
- **In scope:** append logs, read windows for graphs, retention cleanup.
- **Out:** graph rendering itself.
- **Files:** `app/src/storage/sd_logger/*`.
- **Deps:** WP-07.
- **Source refs:** `sd_card/sd_card.cpp`.
- **Acceptance:** data append/read path works; retention task bounded.
- **Tests:** filesystem-backed unit tests (`native_sim`) + compile checks.

---

## Phase 6 — Insight variant

### WP-25 — Insight display stack (fonts/QR/screens)
- **Goal:** Port Insight display system and screen navigation.
- **In scope:** display init/update loop, basic screen set, QR rendering, font assets.
- **Out:** Urban analytics fusion details.
- **Files:** `app/src/insight/display/*`, `app/assets/fonts/*`, `app/assets/qr/*`.
- **Deps:** WP-03, WP-21, WP-23.
- **Source refs:** `display/display_manager.cpp`, `display/*`, `icons/*`.
- **Acceptance:** Insight UI navigable with buttons and renders key pages.
- **Tests:** compile-time tests + pure logic tests for screen FSM.

### WP-26 — Insight Urban aggregation + analytics integration
- **Goal:** Complete Insight-specific aggregation workflows and analytics views.
- **In scope:** Urban pairing mode, cached Urban identity, graph/analytics data flow.
- **Out:** non-Insight features.
- **Files:** `app/src/insight/aggregation/*`, `app/src/insight/analytics/*`.
- **Deps:** WP-19, WP-24, WP-25.
- **Source refs:** `sensors/http_altruist_sensor.cpp`, `display/display_manager.cpp`, `airrohr-firmware.ino`.
- **Acceptance:** Insight paired mode and standalone mode both operate as expected.
- **Tests:** logic tests (`native_sim`) + compile checks for Insight profile.

---

## Suggested parallelization graph

- Earliest parallel lane after WP-07: WP-08..WP-16 can run in parallel.
- Networking lane: WP-17 and WP-18 in parallel after crypto + framework.
- UI lane: WP-20/WP-21 parallel with networking completion.
- Insight lane: WP-25 starts after core UI/button/i18n; WP-26 after WP-19 + WP-24 + WP-25.

## Source baseline index (non-exhaustive)

- Core runtime: `airrohr-firmware.ino`
- Build/channels: `platformio.ini`, `platformio_build_metadata.py`
- Config: `config_manager/config_helpers.cpp`, `config_manager/config_defaults.h`, `config_manager/airrohr-cfg.h`
- Sensors: `sensors/sensor.h`, `sensors/sensor_factory.h`, `sensors/*.cpp`
- APIs: `apis/api.h`, `apis/robonomics_datalog_api.cpp`, `apis/robonomics_http_api.cpp`, `apis/helpers/message_formatter.cpp`
- Networking/UI: `wifi_manager.cpp`, `webserver/webserver.cpp`, `intl.h`, `translations/intl_en.h`, `translations/intl_ru.h`
- OTA/partitions: `OTA_Update.cpp`, `esp32c3_partitions.csv`, `esp32c6_partitions.csv`
- HW UI: `leds/leds_controller_urban.cpp`, `buttons/button_manager.cpp`, `display/display_manager.cpp`, `sd_card/sd_card.cpp`

## GPL-3.0 note

When implementation packages copy/adapt logic from source firmware, preserve GPL-3.0-compatible licensing and attributions.
