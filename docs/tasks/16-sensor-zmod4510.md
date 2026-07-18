# WP-16: Sensor ZMOD4510

## Agent task type
Documentation-guided implementation package for Zephyr porting.

## Context
You are implementing one package from the Altruist firmware Zephyr-port roadmap.

- Source firmware: `https://github.com/airalab/altruist-firmware` (branch `esp32`)
- Target repository: `akagi-dev/altruist-zephyr-firmware`
- Phase: **Phase 2**
- Dependencies: **07**

This task must be completed **without** relying on other package files except listed dependencies.

## Goal
Port ZMOD4510 gas sensor support.

## Scope
- **In scope:** Implement no2/o3/aqi mapping.
- **Out of scope:** No unrelated sensors.

## Requirements
1. Follow Zephyr LTS3 (v3.7.0) conventions (Kconfig/devicetree/CMake/Twister).
2. Keep implementation modular and variant-gated (`Urban C3 reduced`, `Urban C6`, `Insight C6`).
3. Maintain GPL-3.0 compatibility when adapting logic from source firmware.
4. Add/update tests for behavior introduced by this package.
5. Keep changes focused to this package only.

## Files to create/update (expected)
- `app/src/sensors/zmod4510/*`
- `tests/sensors/zmod4510/*`

## Source pointers (read before coding)
- `airalab/altruist-firmware: sensors/zmod4510_sensor.cpp`
- `airalab/altruist-firmware: sensors/drivers/zmod4510/*`

## Acceptance criteria
- Package goal achieved and integrated behind clear module boundaries.
- Builds for relevant board/profile targets.
- Required tests added and passing (Twister/native_sim where possible).
- No regressions in previously completed dependency packages.

## Suggested tests
- Twister: `native_sim` for pure logic/state machines/parsers.
- Compile-only matrix for affected ESP32 variants.
- If hardware-specific behavior cannot run in Twister, provide deterministic unit-level tests for the non-hardware logic.

## Handoff notes
- Summarize design decisions and unresolved risks in the PR description.
- Explicitly reference any assumptions about Zephyr ESP32 support status.
