# WP-06: Identity/keystore ED25519

## Agent task type
Documentation-guided implementation package for Zephyr porting.

## Context
You are implementing one package from the Altruist firmware Zephyr-port roadmap.

- Source firmware: `https://github.com/airalab/altruist-firmware` (branch `esp32`)
- Target repository: `akagi-dev/altruist-zephyr-firmware`
- Phase: **Phase 1**
- Dependencies: **04**

This task must be completed **without** relying on other package files except listed dependencies.

## Goal
Implement key generation, persistent storage, and signing API.

## Scope
- **In scope:** Generate on first boot and expose sign helper for API modules.
- **Out of scope:** No RPC/extrinsic transport yet.

## Requirements
1. Follow Zephyr 4.4 conventions (Kconfig/devicetree/CMake/Twister).
2. Keep implementation modular and variant-gated (`Urban C3 reduced`, `Urban C6`, `Insight C6`).
3. Maintain GPL-3.0 compatibility when adapting logic from source firmware.
4. Add/update tests for behavior introduced by this package.
5. Keep changes focused to this package only.

## Files to create/update (expected)
- `app/src/crypto/identity.c`
- `app/include/altruist/identity.h`
- `tests/crypto/*`

## Source pointers (read before coding)
- `airalab/altruist-firmware: apis/robonomics_datalog_api.cpp`
- `airalab/altruist-firmware: config_manager/config_helpers.cpp`

## Acceptance criteria
- Package goal achieved and integrated behind clear module boundaries.
- Builds for relevant board/profile targets.
- Required tests added and passing (Twister/native_sim where possible).
- No regressions in previously completed dependency packages.
- Key reset behavior is documented and testable: deleting identity settings forces regeneration on next init.

## Suggested tests
- Twister: `native_sim` for pure logic/state machines/parsers.
- Compile-only matrix for affected ESP32 variants.
- If hardware-specific behavior cannot run in Twister, provide deterministic unit-level tests for the non-hardware logic.

## Handoff notes
- Summarize design decisions and unresolved risks in the PR description.
- Explicitly reference any assumptions about Zephyr ESP32 support status.

## Identity architecture overview

The WP-06 implementation separates identity responsibilities into a focused crypto module with a small API boundary:

- **Public API (`app/include/altruist/identity.h`)**
  - `altruist_identity_init()`: initialize identity state by loading persisted keys or generating a new pair on first boot.
  - `altruist_identity_sign()` / `altruist_identity_verify()`: runtime signing and verification helpers for higher-level report paths.
  - `altruist_identity_get_public_key()`: export current device public key for enrollment/verification flows.
  - `altruist_identity_reset()`: clear persisted identity so next init regenerates a new keypair.
  - Detached vector helpers (`*_detached`) are exposed for deterministic test coverage and module-level verification.

- **Module implementation (`app/src/crypto/identity.c`)**
  - Uses PSA Crypto Ed25519 (`PSA_ALG_PURE_EDDSA`) for key generation, signing, and verification.
  - Keeps identity state internal and synchronized with a module mutex.
  - Converts PSA return codes into Zephyr-style errno values for consistent caller behavior.

- **Persistence seam**
  - Default persistence stores key material in Zephyr Settings under:
    - `altruist/identity/private_key`
    - `altruist/identity/public_key`
  - Weak-link storage hooks (`load/save/reset`) provide an abstraction seam that tests can override and future backends can replace without changing API callers.

- **Startup and integration**
  - Identity initialization is invoked from app boot when `CONFIG_ALTRUIST_IDENTITY` is enabled.
  - Build wiring is contained to app Kconfig/CMake and native_sim Twister coverage for identity behavior.

## Private key security: current state and future directions

### Current solution highlights

- The private key is generated on-device and persisted to Settings for continuity across reboots.
- The storage backend is abstracted behind weak hooks, which keeps migration path open.
- Key reset behavior is explicit and test-covered: reset removes persisted identity and forces regeneration.

### Security limitation in current approach

- The default implementation persists the **raw private key bytes** in Settings.
- If the selected Settings backend does not provide encryption/access controls, private key extraction from flash/filesystem images is possible under physical access or firmware image extraction scenarios.

### Candidate future improvements

1. **PSA persistent key handle model**
   - Keep private key material inside PSA-managed secure storage using a fixed key ID.
   - Persist only key metadata/ID (or just public key) in Settings.

2. **Encrypted-at-rest key blob**
   - Encrypt persisted private key using a hardware-unique root key / secure element / SoC-protected secret.
   - Decrypt only in-memory for signing operations.

3. **Hardware-backed keystore path**
   - Prefer secure element, TrustZone/TEE, or vendor HSM capability where available.
   - Restrict direct private key export and use sign-by-handle only.

4. **Threat-model-driven profile gating**
   - Keep current raw-settings path for development/native_sim profiles.
   - Provide stronger storage mode as default for production/security-sensitive profiles.
