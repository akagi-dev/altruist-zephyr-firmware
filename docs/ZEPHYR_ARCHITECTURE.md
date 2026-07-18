# Target Zephyr Architecture (LTS3 / v3.7.0)

This is the target architecture for porting Altruist firmware functionality from Arduino/PlatformIO to Zephyr OS.

## 1) Workspace and repository layout

## West workspace

- Root workspace manifest in `west.yml`.
- Pin Zephyr `LTS3 (v3.7.0)` and required external modules (if any) in west projects.
- Keep application code in a dedicated app directory (recommended `app/`).

Proposed structure:

- `west.yml`
- `app/CMakeLists.txt`
- `app/Kconfig`
- `app/prj.conf`
- `app/src/...`
- `app/include/...`
- `app/boards/...` (if custom Altruist board defs are local)
- `tests/...` (Twister-friendly unit/integration tests)

## Board targets

- Development baseline: `esp32c6_devkitc`.
- Legacy/size-constrained Urban profile: ESP32-C3 board target (plus reduced feature Kconfig set).
- Custom Altruist board definitions when needed:
  - devicetree pin mapping (UART/I2C/I2S/LED strip/buttons/display)
  - Kconfig defaults per board/variant.

## 2) Subsystem mapping: source firmware → Zephyr

| Source subsystem | Current source refs | Zephyr target |
|---|---|---|
| Sensor abstraction/factory | `sensors/sensor.h`, `sensors/sensor_factory.h` | `app/src/sensors/*` + Zephyr sensor API + DT aliases/chosen nodes |
| UART/I2C drivers | `sensors/*_sensor.cpp` | Zephyr `uart`, `i2c`, `sensor` drivers; out-of-tree shims where no upstream driver exists |
| WiFi manager/captive flow | `wifi_manager.cpp` | `net_mgmt` WiFi APIs, connection manager, dedicated provisioning module |
| Web server/config pages | `webserver/webserver.cpp`, `webserver/pages/*` | Zephyr HTTP server + REST/config handlers + static assets |
| SPIFFS config JSON | `config_manager/*` | Zephyr Settings subsystem + NVS or LittleFS backend |
| ED25519 signing identity | `apis/robonomics_datalog_api.cpp`, `config_helpers.cpp` | PSA Crypto (mbedTLS backend) or compact ED25519 library with settings-stored private key |
| Robonomics datalog API | `apis/robonomics_datalog_api.cpp` | Zephyr network client + portable SCALE/extrinsic C module |
| Connectivity POST :65 | `apis/robonomics_http_api.cpp` | Zephyr HTTP client + server pool logic |
| mDNS advertisement/discovery | `wifi_manager.cpp`, `sensors/http_altruist_sensor.cpp` | Zephyr mDNS responder/query integration |
| OTA update | `OTA_Update.cpp`, `esp32c*_partitions.csv` | MCUboot dual-slot DFU + hawkBit/custom HTTP updater |
| LED indicators | `leds/leds_controller_urban.cpp` | Zephyr `led_strip` (WS2812 via RMT/SPI) |
| Buttons | `buttons/button_manager.cpp` | GPIO interrupts + debounce/event module |
| SD card CSV/rollups | `sd_card/sd_card.cpp` | Zephyr FS + storage backend + worker thread |
| Insight display/screens/QR | `display/display_manager.cpp` | CFB or LVGL stack + display scheduler + asset pipeline |
| Improv serial provisioning | `improv/improv_serial.cpp` | custom protocol module over UART (and optional BLE extension) |

## 3) Threading and execution model

Replace Arduino `loop()` + FreeRTOS task mix with explicit Zephyr work model:

- **sensor_work_q**: periodic sensor polling work items.
- **api_work_q**:
  - connectivity sender every 30s
  - datalog sender every 10m
- **net_mgmt event handler**: WiFi link state, reconnect, IP acquisition.
- **web server thread**: configuration/status API.
- **ui/display thread** (Insight only): screen state machine and button events.
- **ota/dfu worker**: update checks/download/install.

Use message queues (`k_msgq`) or ring buffers for sensor snapshot handoff to API/UI without sharing mutable global JSON state.

## 4) Data model and persistence

- Replace monolithic Arduino JSON document with typed C structs for sensor snapshots.
- Preserve compatibility-facing payload format for Robonomics map/datalog encoders.
- Persist config and identity using Zephyr Settings keys:
  - `altruist/wifi/*`
  - `altruist/robonomics/*`
  - `altruist/sensors/*`
  - `altruist/ui/*`

## 5) Crypto and Substrate pipeline

- ED25519 signing via PSA Crypto where practical; fallback to audited compact ED25519 C implementation.
- Implement a **small portable SCALE codec + extrinsic builder module** (no mature Zephyr-native Substrate client currently available).
- Keep module isolated under `app/src/substrate/` with test coverage from known vectors.

## 6) OTA architecture

- MCUboot dual-slot image layout (`slot0` + `slot1`) on ESP32-C6/C3.
- Signed image validation enabled.
- Stable/testing channels managed by runtime config + build metadata; explicit rollout policy in updater module.

## 7) Kconfig feature matrix

| Feature | Urban C3 (reduced) | Urban C6 | Insight C6 |
|---|---:|---:|---:|
| WiFi STA + provisioning | ✅ | ✅ | ✅ |
| Captive web config | ✅ | ✅ | ✅ |
| mDNS responder | ⚠️ optional (flash/RAM constrained) | ✅ | ✅ |
| SDS011 | ✅ | ✅ | optional |
| BMx280 | ✅ | ✅ | ✅ |
| BME680 | ❌ | optional | ✅ |
| SCD4x | ✅ | ✅ | ✅ |
| RadSens | ✅ | ✅ | optional |
| I2S noise | ✅ (budget-dependent) | ✅ | optional |
| AGS3871 | ❌ (default off) | optional | optional |
| ZMOD4510 | ❌ (default off) | optional | optional |
| GPS | ❌ default off | optional | optional |
| HTTP Altruist sensor | ❌ | optional | ✅ |
| Display/LVGL | ❌ | ❌ | ✅ |
| QR/font assets | ❌ | ❌ | ✅ |
| LED strip | optional | ✅ | optional |
| SD card logging | ❌ | optional | ✅ |
| OTA (MCUboot) | ✅ | ✅ | ✅ |

## 8) Memory budget guidance (initial)

- **Urban C3 target**: aggressively modular build profile; disable display, heavier sensors, and optional protocols by default.
- **Urban C6 target**: full urban feature set including LED and mDNS.
- **Insight C6 target**: largest RAM/flash use (display, fonts, QR, graph history, Urban aggregation).

Introduce explicit per-profile budgets in CI (ROM/RAM thresholds from `west build --cmake-only` + size report parsing).

## 9) Key risks and mitigations

1. **ESP32-C6 WiFi maturity in Zephyr**
   - Risk: reconnect/captive flows and long-run stability may differ from Arduino stack.
   - Mitigation: early soak tests and robust net_mgmt reconnection state machine.

2. **ESP32-C3 flash/RAM pressure**
   - Risk: C3 cannot fit full C6/Insight-equivalent feature set.
   - Mitigation: strict Kconfig pruning profile (Urban C3 reduced).

3. **No off-the-shelf Substrate/SCALE client for Zephyr**
   - Risk: protocol layer complexity and correctness.
   - Mitigation: minimal standalone C codec + vectors + staged integration.

4. **Display/asset footprint for Insight**
   - Risk: font/bitmap/QR assets dominate flash.
   - Mitigation: generated assets, compression decisions, and lazy rendering.

5. **OTA migration path differences (SPIFFS → Settings/FS + MCUboot)**
   - Risk: migration and rollback behavior divergence.
   - Mitigation: explicit migration package with rollback acceptance tests.

## 10) GPL-3.0 compatibility note

Porting tasks that adapt logic from `airalab/altruist-firmware` should preserve GPL-3.0-compatible licensing and attribution in corresponding source files and docs.
