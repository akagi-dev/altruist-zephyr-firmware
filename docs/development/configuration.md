# Configuration lifecycle

This page documents the WP-04 configuration subsystem for developers integrating runtime config with Zephyr Settings.

## Module boundary

- Public API: `app/include/altruist/config.h`
- Implementation: `app/src/config/config_store.c`
- Defaults provider: `app/src/config/config_defaults.c`

Main API:

- `altruist_config_init()`: apply defaults, initialize Settings, load persisted values, run migration if needed.
- `altruist_config_load()`: load `altruist/*` subtree from Settings backend into runtime state.
- `altruist_config_save()`: persist current runtime state via Zephyr Settings.
- `altruist_config_reset_defaults(bool persist)`: restore default values in RAM, optionally persist.
- `altruist_config_replace(const struct altruist_config *new_cfg, bool persist)`: replace runtime config, optionally persist.
- `altruist_config_get()`: snapshot-style read access to current config.
- `altruist_config_set_migration_hook()`: register schema migration callback.

## Config structure

`struct altruist_config` is a stable contract containing:

- Schema/version (`schema_version`)
- Language/network fields (`current_lang`, Wi-Fi STA/AP credentials)
- Robonomics/connectivity endpoints and owner fields
- Feature flags (`send2robonomics`, `send2csv`, `standalone`)
- Timing values (`sending_interval_ms`, `datalog_sending_interval_ms`, `sds_meas_interval_ms`, `time_for_wifi_config`)

Field sizes are bounded by constants in `config.h` (for example `ALTRUIST_CONFIG_SSID_MAX_LEN`, `ALTRUIST_CONFIG_URL_MAX_LEN`) to keep cross-module behavior stable.

## Zephyr Settings integration

Configuration is stored under the `altruist` root in Zephyr Settings:

- Example keys: `altruist/schema_version`, `altruist/wlanssid`, `altruist/send2csv`, `altruist/sending_interval_ms`
- Runtime handler is registered with `SETTINGS_STATIC_HANDLER_DEFINE(...)`
- Boolean values are normalized to `uint8_t` encoding (`0`/`1`) for persistence compatibility
- Canonical interval keys are supported with legacy compatibility in load path:
  - `sending_interval_ms` + `sending_intervall_ms`
  - `datalog_sending_interval_ms` + `datalog_sending_intervall_ms`

## Typical usage scenarios

### Startup load flow

1. Call `altruist_config_init()` during app startup.
2. Defaults are applied first.
3. Settings subsystem initializes and persisted values are loaded.
4. Migration hook runs if `schema_version` is older than current.
5. Updated schema is saved back if migration happened.

### Runtime update and persist flow

1. Read current snapshot with `altruist_config_get()`.
2. Build an updated `struct altruist_config`.
3. Apply using `altruist_config_replace(&cfg, true)` to persist immediately.

### Reset to defaults

- `altruist_config_reset_defaults(false)` restores defaults in RAM only.
- `altruist_config_reset_defaults(true)` restores defaults and persists through Settings.

## Working with Zephyr APIs directly

The config module owns the Settings handler and wraps core operations:

- `settings_subsys_init()` during config init
- `settings_load_subtree("altruist")` for loading config keys
- `settings_save()` for persistence

Use the config API as the primary interface. Access Settings directly only when adding new config keys/serialization paths inside the config module.
