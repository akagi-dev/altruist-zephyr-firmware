# Altruist Zephyr Firmware

This repository contains the Altruist Zephyr workspace application skeleton.
It follows the upstream `zephyrproject-rtos/example-application` workspace
layout and conventions.

## Getting Started

Before getting started, prepare a standard Zephyr development environment using
Zephyr's official getting-started guide.

### Initialize workspace

```sh
# initialize workspace from this manifest repo
west init -m https://github.com/akagi-dev/altruist-zephyr-firmware --mr main my-workspace
cd my-workspace
west update
west zephyr-export
```

### Build

```sh
cd altruist-zephyr-firmware
west build -b esp32c6_devkitc/esp32c6/hpcore app
```

### Twister smoke tests

```sh
west twister -T tests/smoke/native_sim --integration
```

### Variant builds

```sh
west build -b esp32c3_devkitm/riscv/esp32c3 app -- -DEXTRA_CONF_FILE=prj_urban_c3.conf
west build -b esp32c6_devkitc/esp32c6/hpcore app -- -DEXTRA_CONF_FILE=prj_urban_c6.conf
west build -b esp32c6_devkitc/esp32c6/hpcore app -- -DEXTRA_CONF_FILE=prj_insight_c6.conf
```
