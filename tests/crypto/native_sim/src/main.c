/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <altruist/identity.h>

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <psa/crypto.h>
#include <zephyr/ztest.h>

#define TEST_IDENTITY_KEY_ID (PSA_KEY_ID_USER_MIN + 0x0060)

static int stored_save_calls;

int altruist_identity_storage_load(uint8_t private_key[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE],
				   uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE])
{
	psa_key_id_t key_handle = 0;
	psa_status_t status;
	size_t private_key_len;
	size_t public_key_len;

	status = psa_crypto_init();
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	status = psa_open_key(TEST_IDENTITY_KEY_ID, &key_handle);
	if (status == PSA_ERROR_DOES_NOT_EXIST) {
		return -ENOENT;
	}
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	status = psa_export_key(key_handle, private_key, ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE,
				&private_key_len);
	if ((status != PSA_SUCCESS) ||
	    (private_key_len != ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE)) {
		(void)psa_close_key(key_handle);
		return -EIO;
	}

	status = psa_export_public_key(key_handle, public_key, ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE,
				       &public_key_len);
	if ((status != PSA_SUCCESS) ||
	    (public_key_len != ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE)) {
		(void)psa_close_key(key_handle);
		return -EIO;
	}

	status = psa_close_key(key_handle);
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	return 0;
}

int altruist_identity_storage_save(
	const uint8_t private_key[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE],
	const uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE])
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id = TEST_IDENTITY_KEY_ID;
	psa_status_t status;
	uint8_t derived_public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
	size_t public_key_len;

	status = psa_crypto_init();
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_EXPORT);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attributes, 255);
	psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_PERSISTENT);
	psa_set_key_id(&attributes, key_id);

	status = psa_destroy_key(key_id);
	if ((status != PSA_SUCCESS) && (status != PSA_ERROR_DOES_NOT_EXIST)) {
		psa_reset_key_attributes(&attributes);
		return -EIO;
	}

	status = psa_import_key(&attributes, private_key,
				ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE, &key_id);
	psa_reset_key_attributes(&attributes);
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	status = psa_export_public_key(key_id, derived_public_key, sizeof(derived_public_key),
				       &public_key_len);
	if ((status != PSA_SUCCESS) ||
	    (public_key_len != ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE)) {
		(void)psa_destroy_key(key_id);
		return -EIO;
	}

	if (memcmp(derived_public_key, public_key, sizeof(derived_public_key)) != 0) {
		(void)psa_destroy_key(key_id);
		return -EINVAL;
	}

	stored_save_calls++;

	return 0;
}

int altruist_identity_storage_reset(void)
{
	psa_status_t status;

	status = psa_crypto_init();
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	status = psa_destroy_key(TEST_IDENTITY_KEY_ID);
	if ((status != PSA_SUCCESS) && (status != PSA_ERROR_DOES_NOT_EXIST)) {
		return -EIO;
	}

	return 0;
}

static void reset_test_storage(void)
{
	psa_status_t status;

	status = psa_crypto_init();
	if (status == PSA_SUCCESS) {
		(void)psa_destroy_key(TEST_IDENTITY_KEY_ID);
	}

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

/*
 * Test vector from Polkadot SDK for public key derivation from seed.
 * Source: https://github.com/paritytech/polkadot-sdk/blob/master/substrate/primitives/core/src/ed25519.rs
 * Test: test_vector_should_work
 */
ZTEST(identity_sign_verify, test_polkadot_public_key_derivation)
{
	/* Test vector: seed "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60" */
	static const uint8_t seed[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE] = {
		0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60,
		0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4,
		0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19,
		0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60,
	};
	/* Expected public key: "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a" */
	static const uint8_t expected_public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE] = {
		0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7,
		0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
		0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25,
		0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a,
	};
	uint8_t derived_public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id = 0;
	psa_status_t status;
	size_t exported_length;

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	zassert_equal(status, PSA_SUCCESS, "PSA Crypto initialization failed");

	/* Configure key attributes for Ed25519 */
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_EXPORT);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attributes, 255);

	/* Import the seed as private key */
	status = psa_import_key(&attributes, seed, sizeof(seed), &key_id);
	zassert_equal(status, PSA_SUCCESS, "Failed to import private key");

	/* Export the public key */
	status = psa_export_public_key(key_id, derived_public_key,
				       sizeof(derived_public_key), &exported_length);
	zassert_equal(status, PSA_SUCCESS, "Failed to export public key");
	zassert_equal(exported_length, ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE,
		      "Public key length mismatch");

	/* Verify the derived public key matches the expected value */
	zassert_mem_equal(expected_public_key, derived_public_key,
			  sizeof(expected_public_key),
			  "Derived public key does not match expected value");

	/* Cleanup */
	psa_destroy_key(key_id);
	psa_reset_key_attributes(&attributes);
}

/*
 * Test vector from Polkadot SDK for sign/verify with empty message.
 * Source: https://github.com/paritytech/polkadot-sdk/blob/master/substrate/primitives/core/src/ed25519.rs
 * Test: test_vector_should_work
 */
ZTEST(identity_sign_verify, test_polkadot_sign_verify_empty_message)
{
	/* Seed: "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60" */
	static const uint8_t private_key[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE] = {
		0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60,
		0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4,
		0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19,
		0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60,
	};
	/* Public key: "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a" */
	static const uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE] = {
		0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7,
		0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
		0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25,
		0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a,
	};
	/* Expected signature for empty message */
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

	/* Sign the empty message (NULL with length 0) */
	zassert_ok(altruist_identity_sign_detached(private_key, sizeof(private_key),
						   NULL, 0,
						   signature, sizeof(signature)),
		   "Failed to sign message");

	/* Verify signature matches expected value */
	zassert_mem_equal(expected_signature, signature, sizeof(signature),
			  "Signature does not match expected value");

	/* Verify the signature */
	zassert_ok(altruist_identity_verify_detached(public_key, sizeof(public_key),
						     NULL, 0,
						     signature, sizeof(signature)),
		   "Signature verification failed");
}

/*
 * Test vector from Polkadot SDK for seeded pair sign/verify.
 * Source: https://github.com/paritytech/polkadot-sdk/blob/master/substrate/primitives/core/src/ed25519.rs
 * Test: seeded_pair_should_work
 */
ZTEST(identity_sign_verify, test_polkadot_seeded_pair)
{
	/* Seed: "12345678901234567890123456789012" (32 bytes) */
	static const uint8_t seed[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE] = {
		'1', '2', '3', '4', '5', '6', '7', '8',
		'9', '0', '1', '2', '3', '4', '5', '6',
		'7', '8', '9', '0', '1', '2', '3', '4',
		'5', '6', '7', '8', '9', '0', '1', '2',
	};
	/* Expected public key: "2f8c6129d816cf51c374bc7f08c3e63ed156cf78aefb4a6550d97b87997977ee" */
	static const uint8_t expected_public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE] = {
		0x2f, 0x8c, 0x61, 0x29, 0xd8, 0x16, 0xcf, 0x51,
		0xc3, 0x74, 0xbc, 0x7f, 0x08, 0xc3, 0xe6, 0x3e,
		0xd1, 0x56, 0xcf, 0x78, 0xae, 0xfb, 0x4a, 0x65,
		0x50, 0xd9, 0x7b, 0x87, 0x99, 0x79, 0x77, 0xee,
	};
	/* Message from Polkadot SDK test */
	static const uint8_t message[] = {
		0x2f, 0x8c, 0x61, 0x29, 0xd8, 0x16, 0xcf, 0x51,
		0xc3, 0x74, 0xbc, 0x7f, 0x08, 0xc3, 0xe6, 0x3e,
		0xd1, 0x56, 0xcf, 0x78, 0xae, 0xfb, 0x4a, 0x65,
		0x50, 0xd9, 0x7b, 0x87, 0x99, 0x79, 0x77, 0xee,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
		0x00, 0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a,
		0xb7, 0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07,
		0x3a, 0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23,
		0x25, 0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51,
		0x1a, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00,
	};
	uint8_t derived_public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
	uint8_t signature[ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE];
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id = 0;
	psa_status_t status;
	size_t exported_length;

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	zassert_equal(status, PSA_SUCCESS, "PSA Crypto initialization failed");

	/* Configure key attributes for Ed25519 */
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_EXPORT);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attributes, 255);

	/* Import the seed as private key */
	status = psa_import_key(&attributes, seed, sizeof(seed), &key_id);
	zassert_equal(status, PSA_SUCCESS, "Failed to import private key");

	/* Export the public key */
	status = psa_export_public_key(key_id, derived_public_key,
				       sizeof(derived_public_key), &exported_length);
	zassert_equal(status, PSA_SUCCESS, "Failed to export public key");
	zassert_equal(exported_length, ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE,
		      "Public key length mismatch");

	/* Verify the derived public key matches the expected value */
	zassert_mem_equal(expected_public_key, derived_public_key,
			  sizeof(expected_public_key),
			  "Derived public key does not match expected value");

	/* Cleanup - PSA key is no longer needed for signing since we use seed directly */
	psa_destroy_key(key_id);
	psa_reset_key_attributes(&attributes);

	/* Sign the message and verify using the seed (not the PSA key) */
	zassert_ok(altruist_identity_sign_detached(seed, sizeof(seed),
						   message, sizeof(message),
						   signature, sizeof(signature)),
		   "Failed to sign message");

	/* Verify the signature */
	zassert_ok(altruist_identity_verify_detached(expected_public_key,
						     sizeof(expected_public_key),
						     message, sizeof(message),
						     signature, sizeof(signature)),
		   "Signature verification failed");

	/* Verify that wrong message fails verification */
	static const uint8_t wrong_message[] = "Other message";
	zassert_not_equal(altruist_identity_verify_detached(expected_public_key,
							    sizeof(expected_public_key),
							    wrong_message,
							    sizeof(wrong_message) - 1,
							    signature, sizeof(signature)),
			  0, "Signature should not verify with wrong message");
}

ZTEST(identity_sign_verify, test_persistence_and_reset_lifecycle)
{
	const uint8_t *test_message = (const uint8_t *)"altruist-identity-persistence";
	size_t message_len = strlen((const char *)test_message);
	uint8_t public_key_first[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
	uint8_t public_key_second[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
	uint8_t public_key_after_reset[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
	uint8_t signature_a[ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE];
	uint8_t signature_b[ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE];

	reset_test_storage();
	altruist_identity_test_reset_state();

	zassert_ok(altruist_identity_init());
	zassert_equal(stored_save_calls, 1, "key should be generated once");
	zassert_ok(altruist_identity_get_public_key(public_key_first, sizeof(public_key_first)));

	altruist_identity_test_reset_state();
	zassert_ok(altruist_identity_init());
	zassert_equal(stored_save_calls, 1, "reboot load should not generate a new key");
	zassert_ok(altruist_identity_get_public_key(public_key_second, sizeof(public_key_second)));
	zassert_mem_equal(public_key_first, public_key_second, sizeof(public_key_first),
			  "public key did not survive reboot simulation");

	zassert_ok(altruist_identity_sign(test_message, message_len,
					  signature_a, sizeof(signature_a)));
	zassert_ok(altruist_identity_sign(test_message, message_len,
					  signature_b, sizeof(signature_b)));
	zassert_mem_equal(signature_a, signature_b, sizeof(signature_a),
			  "ed25519 signature must be deterministic");
	zassert_ok(altruist_identity_verify(test_message, message_len,
					    signature_a, sizeof(signature_a)));

	zassert_ok(altruist_identity_reset());
	zassert_equal(stored_save_calls, 2, "reset should regenerate and persist a new key");
	zassert_ok(altruist_identity_get_public_key(public_key_after_reset, sizeof(public_key_after_reset)));
	zassert_true(memcmp(public_key_first, public_key_after_reset, sizeof(public_key_first)) != 0,
		     "reset should force identity regeneration");
	zassert_ok(altruist_identity_sign(test_message, message_len,
					  signature_a, sizeof(signature_a)));
	zassert_ok(altruist_identity_verify(test_message, message_len,
					    signature_a, sizeof(signature_a)));
}

ZTEST_SUITE(identity_sign_verify, NULL, NULL, NULL, NULL, NULL);
