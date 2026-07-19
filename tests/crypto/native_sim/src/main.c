/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <altruist/identity.h>

#include <errno.h>
#include <string.h>

#include <psa/crypto.h>
#include <zephyr/ztest.h>

/* Match production identity key ID so lifecycle tests exercise the same PSA slot. */
#define TEST_IDENTITY_KEY_ID (PSA_KEY_ID_USER_MIN + 0x0006)

static int sign_with_seed(const uint8_t seed[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE],
			  const uint8_t *message, size_t message_len,
			  uint8_t signature[ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE])
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id = 0;
	psa_status_t status;
	size_t signature_len;
	int rc = 0;

	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attributes, 255);

	status = psa_import_key(&attributes, seed,
				ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE, &key_id);
	psa_reset_key_attributes(&attributes);
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	status = psa_sign_message(key_id, PSA_ALG_PURE_EDDSA, message, message_len, signature,
				  ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE, &signature_len);
	if ((status != PSA_SUCCESS) ||
	    (signature_len != ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE)) {
		rc = -EIO;
	}

	status = psa_destroy_key(key_id);
	if (status != PSA_SUCCESS) {
		rc = -EIO;
	}

	return rc;
}

static int verify_with_public_key(
	const uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE], const uint8_t *message,
	size_t message_len, const uint8_t signature[ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE])
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id = 0;
	psa_status_t status;
	int rc = 0;

	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_MESSAGE);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attributes, 255);

	status = psa_import_key(&attributes, public_key,
				ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE, &key_id);
	psa_reset_key_attributes(&attributes);
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	status = psa_verify_message(key_id, PSA_ALG_PURE_EDDSA, message, message_len, signature,
				    ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE);
	if (status == PSA_ERROR_INVALID_SIGNATURE) {
		rc = -EINVAL;
	} else if (status != PSA_SUCCESS) {
		rc = -EIO;
	}

	status = psa_destroy_key(key_id);
	if (status != PSA_SUCCESS) {
		rc = -EIO;
	}

	return rc;
}

static int derive_public_key_from_seed(
	const uint8_t seed[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE],
	uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE])
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id = 0;
	psa_status_t status;
	size_t public_key_len;
	int rc = 0;

	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_EXPORT);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attributes, 255);

	status = psa_import_key(&attributes, seed,
				ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE, &key_id);
	psa_reset_key_attributes(&attributes);
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	status = psa_export_public_key(key_id, public_key,
				       ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE, &public_key_len);
	if ((status != PSA_SUCCESS) ||
	    (public_key_len != ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE)) {
		rc = -EIO;
	}

	status = psa_destroy_key(key_id);
	if (status != PSA_SUCCESS) {
		rc = -EIO;
	}

	return rc;
}

static void reset_test_storage(void)
{
	psa_status_t status;

	status = psa_crypto_init();
	zassert_equal(status, PSA_SUCCESS, "PSA Crypto initialization failed");

	status = psa_destroy_key(TEST_IDENTITY_KEY_ID);
	zassert_true((status == PSA_SUCCESS) || (status == PSA_ERROR_DOES_NOT_EXIST),
		     "failed to clear persistent identity key");
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

	zassert_ok(sign_with_seed(private_key, NULL, 0U, signature));
	zassert_mem_equal(expected_signature, signature, sizeof(signature), "signature mismatch");
	zassert_ok(verify_with_public_key(public_key, NULL, 0U, signature));

	signature[0] ^= 0x01U;
	zassert_not_equal(verify_with_public_key(public_key, NULL, 0U, signature), 0,
			  "tampered signature unexpectedly verified");
}

ZTEST(identity_sign_verify, test_polkadot_public_key_derivation)
{
	static const uint8_t seed[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE] = {
		0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60,
		0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4,
		0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19,
		0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60,
	};
	static const uint8_t expected_public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE] = {
		0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7,
		0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
		0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25,
		0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a,
	};
	uint8_t derived_public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];

	zassert_ok(derive_public_key_from_seed(seed, derived_public_key));
	zassert_mem_equal(expected_public_key, derived_public_key, sizeof(expected_public_key),
			  "derived public key does not match expected value");
}

ZTEST(identity_sign_verify, test_persistence_and_reset_lifecycle)
{
	const uint8_t *test_message = (const uint8_t *)"altruist-identity-persistence";
	size_t message_len = strlen((const char *)test_message);
	uint8_t public_key_first[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
	uint8_t public_key_second[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
	uint8_t public_key_after_reset[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
	uint8_t public_key_after_reset_reboot[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
	uint8_t signature_a[ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE];
	uint8_t signature_b[ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE];
	psa_key_id_t key_handle = 0;
	psa_status_t status;

	reset_test_storage();
	altruist_identity_test_reset_state();

	zassert_ok(altruist_identity_init());
	zassert_ok(altruist_identity_get_public_key(public_key_first, sizeof(public_key_first)));
	status = psa_open_key(TEST_IDENTITY_KEY_ID, &key_handle);
	zassert_equal(status, PSA_SUCCESS, "persistent identity key missing after init");
	status = psa_close_key(key_handle);
	zassert_equal(status, PSA_SUCCESS, "failed to close persistent identity key");

	altruist_identity_test_reset_state();
	zassert_ok(altruist_identity_init());
	zassert_ok(altruist_identity_get_public_key(public_key_second, sizeof(public_key_second)));
	zassert_mem_equal(public_key_first, public_key_second, sizeof(public_key_first),
			  "public key did not survive reboot simulation");

	zassert_ok(altruist_identity_sign(test_message, message_len, signature_a, sizeof(signature_a)));
	zassert_ok(altruist_identity_sign(test_message, message_len, signature_b, sizeof(signature_b)));
	zassert_mem_equal(signature_a, signature_b, sizeof(signature_a),
			  "ed25519 signature must be deterministic");
	zassert_ok(altruist_identity_verify(test_message, message_len, signature_a, sizeof(signature_a)));

	zassert_ok(altruist_identity_reset());
	zassert_ok(altruist_identity_get_public_key(public_key_after_reset,
						    sizeof(public_key_after_reset)));
	zassert_true(memcmp(public_key_first, public_key_after_reset, sizeof(public_key_first)) != 0,
		     "reset should force identity regeneration");
	zassert_ok(altruist_identity_sign(test_message, message_len, signature_a, sizeof(signature_a)));
	zassert_ok(altruist_identity_verify(test_message, message_len, signature_a, sizeof(signature_a)));

	altruist_identity_test_reset_state();
	zassert_ok(altruist_identity_init());
	zassert_ok(altruist_identity_get_public_key(public_key_after_reset_reboot,
						    sizeof(public_key_after_reset_reboot)));
	zassert_mem_equal(public_key_after_reset, public_key_after_reset_reboot,
			  sizeof(public_key_after_reset), "regenerated key should persist across reboot");
}

ZTEST_SUITE(identity_sign_verify, NULL, NULL, NULL, NULL, NULL);
