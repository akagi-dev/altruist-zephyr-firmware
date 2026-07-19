# Device Identity

This page documents the WP-06 identity implementation architecture for developers working on signing/reporting paths and security hardening.

## Module boundary

Identity responsibilities are isolated in a dedicated crypto module:

- API header: `app/include/altruist/identity.h`
- Implementation: `app/src/crypto/identity.c`

Public API:

- `altruist_identity_init()`: load persisted identity or generate on first boot.
- `altruist_identity_sign()` and `altruist_identity_verify()`: Ed25519 sign/verify helpers for report pipelines.
- `altruist_identity_get_public_key()`: access current device public key.
- `altruist_identity_reset()`: remove persisted identity and force regeneration on next init.
- Detached helpers (`*_detached`) for deterministic vector testing.

## Crypto implementation

- Algorithm: Ed25519 via PSA Crypto (`PSA_ALG_PURE_EDDSA`).
- Internal state is module-local and synchronized with a mutex.
- PSA status values are mapped to Zephyr-style errno values for stable caller behavior.

## Persistence architecture

Default storage backend uses Zephyr Settings keys:

- `altruist/identity/private_key`
- `altruist/identity/public_key`

The module exposes weak storage hooks (`load/save/reset`) so tests can override persistence behavior and future production backends can be introduced without changing API consumers.

## Startup integration

- Boot path initializes identity when `CONFIG_ALTRUIST_IDENTITY` is enabled.
- Twister native_sim tests cover vector correctness and lifecycle behavior (generate/persist/reload/reset).

## Private key security

### Current solution highlights

- Private key is generated on-device and persisted for reboot continuity.
- Storage access is abstracted via weak hooks to keep migration/hardening paths open.
- Reset semantics are explicit and test-covered.

### Current limitation

- Default implementation stores raw private key bytes in Settings.
- If Settings backend lacks encryption/access controls, physical extraction of flash/filesystem images can expose key material.

### Future hardening options

1. **PSA persistent key handle flow**
   - Store key in PSA-managed secure storage using fixed key ID.
   - Persist only public key and/or metadata handle.
2. **Encrypted key blob at rest**
   - Encrypt persisted private key with hardware-unique/root secret.
3. **Hardware-backed key storage**
   - Prefer secure element / TEE / HSM-style sign-by-handle flows.
4. **Profile-based security modes**
   - Keep raw-settings mode for development/native_sim, default to hardened mode in production profiles.
