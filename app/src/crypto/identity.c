/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <altruist/identity.h>

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/toolchain.h>

#include <psa/crypto.h>

LOG_MODULE_REGISTER(altruist_identity, CONFIG_LOG_DEFAULT_LEVEL);

#define IDENTITY_PERSISTENT_KEY_ID (PSA_KEY_ID_USER_MIN + 0x0006)
/* PSA Crypto expects 255 for Ed25519 key-bit attributes (curve parameter size). */
#define IDENTITY_ED25519_KEY_BITS 255

struct identity_state {
	bool initialized;
	uint8_t private_key[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE];
	uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
};

static struct identity_state state;
K_MUTEX_DEFINE(identity_lock);

static int identity_generate_keypair(uint8_t *private_key, uint8_t *public_key)
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id = 0;
	psa_status_t status;
	size_t exported_length;
	int rc = 0;

	if ((private_key == NULL) || (public_key == NULL)) {
		return -EINVAL;
	}

	/* Configure key attributes for Ed25519 */
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_VERIFY_MESSAGE |
						  PSA_KEY_USAGE_EXPORT);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attributes, IDENTITY_ED25519_KEY_BITS);

	/* Generate the key pair */
	status = psa_generate_key(&attributes, &key_id);
	if (status != PSA_SUCCESS) {
		psa_reset_key_attributes(&attributes);
		return -EIO;
	}

	/* Export the private key */
	status = psa_export_key(key_id, private_key, ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE,
				&exported_length);
	if ((status != PSA_SUCCESS) ||
	    (exported_length != ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE)) {
		psa_destroy_key(key_id);
		psa_reset_key_attributes(&attributes);
		return -EIO;
	}

	/* Export the public key */
	status = psa_export_public_key(key_id, public_key,
				       ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE,
				       &exported_length);
	if ((status != PSA_SUCCESS) ||
	    (exported_length != ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE)) {
		psa_destroy_key(key_id);
		psa_reset_key_attributes(&attributes);
		return -EIO;
	}

	/* Destroy the volatile key handle */
	psa_destroy_key(key_id);
	psa_reset_key_attributes(&attributes);

	return rc;
}

/*
 * Weak linkage allows tests to provide strong symbol overrides for these storage
 * hooks, so unit tests can validate identity lifecycle without touching the
 * production persistent-key backend.
 */
int __weak altruist_identity_storage_load(uint8_t private_key[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE],
					  uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE])
{
	psa_key_id_t key_id = 0;
	psa_status_t status;
	size_t private_key_len;
	size_t public_key_len;

	if ((private_key == NULL) || (public_key == NULL)) {
		return -EINVAL;
	}

	status = psa_open_key(IDENTITY_PERSISTENT_KEY_ID, &key_id);
	if (status == PSA_ERROR_DOES_NOT_EXIST) {
		return -ENOENT;
	}
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	status = psa_export_key(key_id, private_key, ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE,
				&private_key_len);
	if ((status != PSA_SUCCESS) ||
	    (private_key_len != ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE)) {
		(void)psa_close_key(key_id);
		return -EIO;
	}

	status = psa_export_public_key(key_id, public_key,
				       ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE,
				       &public_key_len);
	if ((status != PSA_SUCCESS) ||
	    (public_key_len != ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE)) {
		(void)psa_close_key(key_id);
		return -EIO;
	}

	status = psa_close_key(key_id);
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	return 0;
}

/* See altruist_identity_storage_load() note about weak test overrides. */
int __weak altruist_identity_storage_save(
	const uint8_t private_key[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE],
	const uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE])
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id = IDENTITY_PERSISTENT_KEY_ID;
	psa_status_t status;
	uint8_t derived_public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
	size_t derived_public_key_len;

	if ((private_key == NULL) || (public_key == NULL)) {
		return -EINVAL;
	}

	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_EXPORT);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attributes, IDENTITY_ED25519_KEY_BITS);
	psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_PERSISTENT);
	psa_set_key_id(&attributes, key_id);

	status = psa_destroy_key(key_id);
	if ((status != PSA_SUCCESS) && (status != PSA_ERROR_DOES_NOT_EXIST)) {
		LOG_ERR("failed to destroy existing persistent key before import (status=%d)",
			(int)status);
		return -EIO;
	}

	status = psa_import_key(&attributes, private_key,
				ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE, &key_id);
	psa_reset_key_attributes(&attributes);
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	status = psa_export_public_key(key_id, derived_public_key,
				       ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE,
				       &derived_public_key_len);
	if ((status != PSA_SUCCESS) ||
	    (derived_public_key_len != ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE)) {
		LOG_ERR("failed to derive public key from imported key (status=%d)", (int)status);
		(void)psa_destroy_key(key_id);
		return -EIO;
	}

	if (memcmp(derived_public_key, public_key, sizeof(derived_public_key)) != 0) {
		LOG_ERR("imported private key does not match provided public key");
		(void)psa_destroy_key(key_id);
		return -EINVAL;
	}

	return 0;
}

/* See altruist_identity_storage_load() note about weak test overrides. */
int __weak altruist_identity_storage_reset(void)
{
	psa_status_t status = psa_destroy_key(IDENTITY_PERSISTENT_KEY_ID);

	if ((status != PSA_SUCCESS) && (status != PSA_ERROR_DOES_NOT_EXIST)) {
		return -EIO;
	}

	return 0;
}

int altruist_identity_sign_detached(const uint8_t *private_key, size_t private_key_len,
				    const uint8_t *message, size_t message_len,
				    uint8_t *signature, size_t signature_len)
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id = 0;
	psa_status_t status;
	size_t signature_length;
	int rc = 0;

	if ((private_key == NULL) ||
	    (private_key_len != ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE) ||
	    ((message == NULL) && (message_len > 0U)) || (signature == NULL) ||
	    (signature_len < ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE)) {
		return -EINVAL;
	}

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	/* Configure key attributes for Ed25519 */
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attributes, IDENTITY_ED25519_KEY_BITS);

	/* Import the private key */
	status = psa_import_key(&attributes, private_key, private_key_len, &key_id);
	if (status != PSA_SUCCESS) {
		psa_reset_key_attributes(&attributes);
		return -EIO;
	}

	/* Sign the message */
	status = psa_sign_message(key_id, PSA_ALG_PURE_EDDSA, message, message_len,
				  signature, signature_len, &signature_length);
	if ((status != PSA_SUCCESS) ||
	    (signature_length != ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE)) {
		rc = -EIO;
	}

	/* Destroy the volatile key handle */
	status = psa_destroy_key(key_id);
	if (status != PSA_SUCCESS) {
		LOG_ERR("failed to destroy temporary signing key (status=%d)", (int)status);
		rc = -EIO;
	}
	psa_reset_key_attributes(&attributes);

	return rc;
}

int altruist_identity_verify_detached(const uint8_t *public_key, size_t public_key_len,
				      const uint8_t *message, size_t message_len,
				      const uint8_t *signature, size_t signature_len)
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id = 0;
	psa_status_t status;
	int rc = 0;

	if ((public_key == NULL) ||
	    (public_key_len != ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE) ||
	    ((message == NULL) && (message_len > 0U)) || (signature == NULL) ||
	    (signature_len != ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE)) {
		return -EINVAL;
	}

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	/* Configure key attributes for Ed25519 public key */
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_MESSAGE);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attributes, IDENTITY_ED25519_KEY_BITS);

	/* Import the public key */
	status = psa_import_key(&attributes, public_key, public_key_len, &key_id);
	if (status != PSA_SUCCESS) {
		psa_reset_key_attributes(&attributes);
		return -EIO;
	}

	/* Verify the signature */
	status = psa_verify_message(key_id, PSA_ALG_PURE_EDDSA, message, message_len,
				    signature, signature_len);
	if (status != PSA_SUCCESS) {
		rc = -EINVAL;
	}

	/* Destroy the volatile key handle */
	status = psa_destroy_key(key_id);
	if (status != PSA_SUCCESS) {
		LOG_ERR("failed to destroy temporary verification key (status=%d)", (int)status);
		rc = -EIO;
	}
	psa_reset_key_attributes(&attributes);

	return rc;
}

int altruist_identity_init(void)
{
	int rc;
	psa_status_t status;

	k_mutex_lock(&identity_lock, K_FOREVER);

	if (state.initialized) {
		k_mutex_unlock(&identity_lock);
		return 0;
	}

	/* Initialize PSA Crypto subsystem */
	status = psa_crypto_init();
	if (status != PSA_SUCCESS) {
		LOG_ERR("PSA Crypto initialization failed (status=%d)", (int)status);
		k_mutex_unlock(&identity_lock);
		return -EIO;
	}

	rc = altruist_identity_storage_load(state.private_key, state.public_key);
	if (rc == -ENOENT) {
		rc = identity_generate_keypair(state.private_key, state.public_key);
		if (rc == 0) {
			rc = altruist_identity_storage_save(state.private_key, state.public_key);
		}
	}

	if (rc == 0) {
		state.initialized = true;
		LOG_DBG("identity initialized");
	} else {
		LOG_ERR("identity initialization failed (rc=%d)", rc);
	}

	k_mutex_unlock(&identity_lock);
	return rc;
}

int altruist_identity_get_public_key(uint8_t *public_key, size_t public_key_len)
{
	if ((public_key == NULL) ||
	    (public_key_len < ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE)) {
		return -EINVAL;
	}

	k_mutex_lock(&identity_lock, K_FOREVER);
	if (!state.initialized) {
		k_mutex_unlock(&identity_lock);
		return -EACCES;
	}

	memcpy(public_key, state.public_key, ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE);
	k_mutex_unlock(&identity_lock);
	return 0;
}

int altruist_identity_sign(const uint8_t *message, size_t message_len,
			   uint8_t *signature, size_t signature_len)
{
	int rc;

	k_mutex_lock(&identity_lock, K_FOREVER);
	if (!state.initialized) {
		k_mutex_unlock(&identity_lock);
		return -EACCES;
	}

	rc = altruist_identity_sign_detached(state.private_key,
					     ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE,
					     message, message_len, signature, signature_len);
	if (rc != 0) {
		LOG_ERR("sign failed (rc=%d)", rc);
	}
	k_mutex_unlock(&identity_lock);
	return rc;
}

int altruist_identity_verify(const uint8_t *message, size_t message_len,
			     const uint8_t *signature, size_t signature_len)
{
	int rc;

	k_mutex_lock(&identity_lock, K_FOREVER);
	if (!state.initialized) {
		k_mutex_unlock(&identity_lock);
		return -EACCES;
	}

	rc = altruist_identity_verify_detached(state.public_key,
					       ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE,
					       message, message_len, signature, signature_len);
	if (rc != 0) {
		LOG_ERR("verify failed (rc=%d)", rc);
	}
	k_mutex_unlock(&identity_lock);
	return rc;
}

int altruist_identity_reset(void)
{
	int rc;

	k_mutex_lock(&identity_lock, K_FOREVER);
	rc = altruist_identity_storage_reset();
	if (rc == 0) {
		rc = identity_generate_keypair(state.private_key, state.public_key);
		if (rc == 0) {
			rc = altruist_identity_storage_save(state.private_key, state.public_key);
		}
	}
	if (rc == 0) {
		state.initialized = true;
		LOG_INF("identity reset complete and key regenerated");
	} else {
		(void)memset(state.private_key, 0, sizeof(state.private_key));
		(void)memset(state.public_key, 0, sizeof(state.public_key));
		state.initialized = false;
		LOG_ERR("identity reset failed (rc=%d)", rc);
	}

	k_mutex_unlock(&identity_lock);
	return rc;
}

#ifdef CONFIG_ZTEST
void altruist_identity_test_reset_state(void)
{
	k_mutex_lock(&identity_lock, K_FOREVER);
	(void)memset(state.private_key, 0, sizeof(state.private_key));
	(void)memset(state.public_key, 0, sizeof(state.public_key));
	state.initialized = false;
	k_mutex_unlock(&identity_lock);
}
#endif
