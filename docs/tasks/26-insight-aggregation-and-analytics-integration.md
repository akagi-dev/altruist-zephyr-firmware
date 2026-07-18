# WP-26: Insight aggregation and analytics integration

## Agent task type
Documentation-guided implementation package for Zephyr porting.

## Context
You are implementing one package from the Altruist firmware Zephyr-port roadmap.

- Source firmware: `https://github.com/airalab/altruist-firmware` (branch `esp32`)
- Target repository: `akagi-dev/altruist-zephyr-firmware`
- Phase: **Phase 6**
- Dependencies: **19,24,25**

This task must be completed **without** relying on other package files except listed dependencies.

## Goal
Complete paired/standalone Insight behavior with Urban data analytics.

## Scope
- **In scope:** Integrate urban pairing cache, analytics ingestion and views.
- **Out of scope:** No unrelated platform bring-up.

## Requirements
1. Follow Zephyr v4.x conventions (Kconfig/devicetree/CMake/Twister).
2. Keep implementation modular and variant-gated (`Urban C3 reduced`, `Urban C6`, `Insight C6`).
3. Maintain GPL-3.0 compatibility when adapting logic from source firmware.
4. Add/update tests for behavior introduced by this package.
5. Keep changes focused to this package only.

## Files to create/update (expected)
- `app/src/insight/aggregation/*`
- `app/src/insight/analytics/*`
- `tests/insight/analytics/*`

## Source pointers (read before coding)
- `airalab/altruist-firmware: sensors/http_altruist_sensor.cpp`
- `airalab/altruist-firmware: display/display_manager.cpp`
- `airalab/altruist-firmware: airrohr-firmware.ino`

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
