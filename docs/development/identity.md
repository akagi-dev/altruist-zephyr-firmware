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

Default storage backend uses PSA persistent key storage with a fixed key ID.

The module exposes weak storage hooks (`load/save/reset`) so tests can override persistence behavior and future production backends can be introduced without changing API consumers.

## Startup integration

- Boot path initializes identity when `CONFIG_ALTRUIST_IDENTITY` is enabled.
- Twister native_sim tests cover vector correctness and lifecycle behavior (generate/persist/reload/reset).

## Private key security

### Current solution highlights

- Private key is generated on-device and persisted in PSA-managed secure storage.
- Identity lifecycle uses weak storage hooks to keep test overrides and backend evolution straightforward.
- Reset semantics are explicit and test-covered.
