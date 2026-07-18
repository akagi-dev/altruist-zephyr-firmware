# Altruist Zephyr Firmware Port Plan

Planning repository for porting Altruist environmental sensor station firmware from Arduino/PlatformIO (`airalab/altruist-firmware`, branch `esp32`) to Zephyr OS.

## Documentation

- [Source architecture baseline](docs/ARCHITECTURE.md)
- [Target Zephyr architecture](docs/ZEPHYR_ARCHITECTURE.md)
- [Master implementation plan](docs/IMPLEMENTATION_PLAN.md)
- [Agent-ready work packages](docs/tasks/)

## Zephyr workspace bootstrap (Nix flake)

This repository now contains a Zephyr `west.yml` workspace manifest pinned to Zephyr `v4.0.0`, matching the pin in `flake.nix`.

1. Enter the development shell:

   ```sh
   nix develop
   ```

2. Initialize west from this repository and fetch Zephyr/modules:

   ```sh
   west init -l .
   west update
   west zephyr-export
   ```

3. Build the baseline app:

   ```sh
   west build -b esp32c6_devkitc app
   ```

4. Run smoke tests on `native_sim`:

   ```sh
   west twister -T tests/smoke/native_sim
   ```

## Work package status

| WP | Title | Phase | Status |
|---:|---|---|---|
| 01 | West workspace and app bootstrap | 0 | Done |
| 02 | CI foundation (Zephyr SDK + Twister) | 0 | Planned |
| 03 | Board and variant profiles | 0 | Planned |
| 04 | Settings/config subsystem | 1 | Planned |
| 05 | WiFi manager and provisioning baseline | 1 | Planned |
| 06 | Identity/keystore ED25519 | 1 | Planned |
| 07 | Sensor framework core | 2 | Planned |
| 08 | Sensor SDS011 | 2 | Planned |
| 09 | Sensor BMx280 | 2 | Planned |
| 10 | Sensor BME680 | 2 | Planned |
| 11 | Sensor SCD4x | 2 | Planned |
| 12 | Sensor RadSens | 2 | Planned |
| 13 | Sensor I2S noise | 2 | Planned |
| 14 | Sensor GPS Neo-6M | 2 | Planned |
| 15 | Sensor AGS3871 | 2 | Planned |
| 16 | Sensor ZMOD4510 | 2 | Planned |
| 17 | Connectivity HTTP API (:65) and server pool | 3 | Planned |
| 18 | Substrate datalog + SCALE codec | 3 | Planned |
| 19 | mDNS and HTTP Altruist aggregation | 3 | Planned |
| 20 | Web configuration API and pages | 4 | Planned |
| 21 | i18n EN/RU baseline | 4 | Planned |
| 22 | OTA with MCUboot channels | 5 | Planned |
| 23 | LEDs, buttons, reset semantics | 5 | Planned |
| 24 | SD card logging and retention | 5 | Planned |
| 25 | Insight display, fonts, QR, screens | 6 | Planned |
| 26 | Insight aggregation and analytics integration | 6 | Planned |

## License

When implementation starts, any adapted/ported source logic from `airalab/altruist-firmware` must preserve GPL-3.0 compatibility and attribution.
