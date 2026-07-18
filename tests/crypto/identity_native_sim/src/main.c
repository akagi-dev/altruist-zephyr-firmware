/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <altruist/identity.h>

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/ztest.h>

static bool stored_key_valid;
static uint8_t stored_private_key[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE];
static uint8_t stored_public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
static int stored_save_calls;

int altruist_identity_storage_load(uint8_t private_key[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE],
				   uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE])
{
	if (!stored_key_valid) {
		return -ENOENT;
	}

	memcpy(private_key, stored_private_key, sizeof(stored_private_key));
	memcpy(public_key, stored_public_key, sizeof(stored_public_key));

	return 0;
}

int altruist_identity_storage_save(
	const uint8_t private_key[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE],
	const uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE])
{
	memcpy(stored_private_key, private_key, sizeof(stored_private_key));
	memcpy(stored_public_key, public_key, sizeof(stored_public_key));
	stored_key_valid = true;
	stored_save_calls++;

	return 0;
}

int altruist_identity_storage_reset(void)
{
	(void)memset(stored_private_key, 0, sizeof(stored_private_key));
	(void)memset(stored_public_key, 0, sizeof(stored_public_key));
	stored_key_valid = false;
	return 0;
}

static void reset_test_storage(void)
{
	(void)memset(stored_private_key, 0, sizeof(stored_private_key));
	(void)memset(stored_public_key, 0, sizeof(stored_public_key));
	stored_key_valid = false;
	stored_save_calls = 0;
}

ZTEST(identity_sign_verify, test_rfc8032_vector)
{
	static const uint8_t private_key[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE] = {
		0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60,
		0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4,
		0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19,
		0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60,
	};
	static const uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE] = {
		0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7,
		0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
		0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25,
		0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a,
	};
	static const uint8_t expected_signature[ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE] = {
		0xe5, 0x56, 0x43, 0x00, 0xc3, 0x60, 0xac, 0x72,
		0x90, 0x86, 0xe2, 0xcc, 0x80, 0x6e, 0x82, 0x8a,
		0x84, 0x87, 0x7f, 0x1e, 0xb8, 0xe5, 0xd9, 0x74,
		0xd8, 0x73, 0xe0, 0x65, 0x22, 0x49, 0x01, 0x55,
		0x5f, 0xb8, 0x82, 0x15, 0x90, 0xa3, 0x3b, 0xac,
		0xc6, 0x1e, 0x39, 0x70, 0x1c, 0xf9, 0xb4, 0x6b,
		0xd2, 0x5b, 0xf5, 0xf0, 0x59, 0x5b, 0xbe, 0x24,
		0x65, 0x51, 0x41, 0x43, 0x8e, 0x7a, 0x10, 0x0b,
	};
	uint8_t signature[ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE];

	zassert_ok(altruist_identity_sign_detached(private_key, sizeof(private_key),
						   NULL, 0U,
						   signature, sizeof(signature)));
	zassert_mem_equal(expected_signature, signature, sizeof(signature),
			  "signature mismatch");
	zassert_ok(altruist_identity_verify_detached(public_key, sizeof(public_key),
						     NULL, 0U,
						     signature, sizeof(signature)));

	signature[0] ^= 0x01U;
	zassert_not_equal(altruist_identity_verify_detached(public_key, sizeof(public_key),
							    NULL, 0U,
							    signature, sizeof(signature)),
			  0, "tampered signature unexpectedly verified");
}

ZTEST(identity_sign_verify, test_persistence_and_reset_lifecycle)
{
	static const uint8_t message[] = "altruist-identity-persistence";
	uint8_t public_key_first[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
	uint8_t public_key_second[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
	uint8_t public_key_after_reset[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
	uint8_t signature_a[ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE];
	uint8_t signature_b[ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE];

	reset_test_storage();
	altruist_identity_test_reset_state();

	zassert_ok(altruist_identity_init());
	zassert_true(stored_key_valid, "key was not persisted");
	zassert_equal(stored_save_calls, 1, "key should be generated once");
	zassert_ok(altruist_identity_get_public_key(public_key_first, sizeof(public_key_first)));

	altruist_identity_test_reset_state();
	zassert_ok(altruist_identity_init());
	zassert_equal(stored_save_calls, 1, "reboot load should not generate a new key");
	zassert_ok(altruist_identity_get_public_key(public_key_second, sizeof(public_key_second)));
	zassert_mem_equal(public_key_first, public_key_second, sizeof(public_key_first),
			  "public key did not survive reboot simulation");

	zassert_ok(altruist_identity_sign(message, sizeof(message) - 1U, signature_a, sizeof(signature_a)));
	zassert_ok(altruist_identity_sign(message, sizeof(message) - 1U, signature_b, sizeof(signature_b)));
	zassert_mem_equal(signature_a, signature_b, sizeof(signature_a),
			  "ed25519 signature must be deterministic");
	zassert_ok(altruist_identity_verify(message, sizeof(message) - 1U,
					    signature_a, sizeof(signature_a)));

	zassert_ok(altruist_identity_reset());
	altruist_identity_test_reset_state();
	zassert_ok(altruist_identity_init());
	zassert_ok(altruist_identity_get_public_key(public_key_after_reset, sizeof(public_key_after_reset)));
	zassert_true(memcmp(public_key_first, public_key_after_reset, sizeof(public_key_first)) != 0,
		     "reset should force identity regeneration");
}

ZTEST_SUITE(identity_sign_verify, NULL, NULL, NULL, NULL, NULL);
