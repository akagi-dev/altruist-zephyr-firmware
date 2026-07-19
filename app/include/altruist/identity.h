/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ALTRUIST_IDENTITY_H_
#define ALTRUIST_IDENTITY_H_

#include <stddef.h>
#include <stdint.h>

#define ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE 32U
#define ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE 32U
#define ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE 64U

int altruist_identity_init(void);
int altruist_identity_get_public_key(uint8_t *public_key, size_t public_key_len);
int altruist_identity_sign(const uint8_t *message, size_t message_len,
			   uint8_t *signature, size_t signature_len);
int altruist_identity_verify(const uint8_t *message, size_t message_len,
			     const uint8_t *signature, size_t signature_len);
/* Resets persisted identity, regenerates/persists a new keypair, and keeps identity initialized. */
int altruist_identity_reset(void);

#ifdef CONFIG_ZTEST
void altruist_identity_test_reset_state(void);
#endif

#endif /* ALTRUIST_IDENTITY_H_ */
