/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <altruist/identity.h>

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <psa/crypto.h>

LOG_MODULE_REGISTER(altruist_identity, CONFIG_LOG_DEFAULT_LEVEL);

/* Internal persistent key slot for device identity. */
#define IDENTITY_PERSISTENT_KEY_ID (PSA_KEY_ID_USER_MIN + 0x0006)
/* PSA Crypto expects 255 for Ed25519 key-bit attributes (curve parameter size). */
#define IDENTITY_ED25519_KEY_BITS 255
#define IDENTITY_KEY_TYPE PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS)

struct identity_state {
	bool initialized;
	uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
};

static struct identity_state state;
K_MUTEX_DEFINE(identity_lock);

static int identity_persistent_key_reset(void)
{
	psa_status_t status = psa_destroy_key(IDENTITY_PERSISTENT_KEY_ID);

	if ((status != PSA_SUCCESS) && (status != PSA_ERROR_DOES_NOT_EXIST)) {
		return -EIO;
	}

	return 0;
}

static int identity_load_public_key(uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE])
{
	psa_key_id_t key_id = 0;
	psa_status_t status;
	size_t public_key_len;
	int rc = 0;

	if (public_key == NULL) {
		return -EINVAL;
	}

	status = psa_open_key(IDENTITY_PERSISTENT_KEY_ID, &key_id);
	if (status == PSA_ERROR_DOES_NOT_EXIST) {
		return -ENOENT;
	}
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	status = psa_export_public_key(key_id, public_key,
				       ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE,
				       &public_key_len);
	if ((status != PSA_SUCCESS) ||
	    (public_key_len != ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE)) {
		rc = -EIO;
	}

	status = psa_close_key(key_id);
	if (status != PSA_SUCCESS) {
		LOG_ERR("failed to close persistent identity key (status=%d)", (int)status);
		rc = -EIO;
	}

	return rc;
}

static int identity_generate_persistent_key(uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE])
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id = IDENTITY_PERSISTENT_KEY_ID;
	psa_status_t status;
	size_t exported_length;
	int rc;

	if (public_key == NULL) {
		return -EINVAL;
	}

	rc = identity_persistent_key_reset();
	if (rc != 0) {
		LOG_ERR("failed to clear previous identity key (rc=%d)", rc);
		return rc;
	}

	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_VERIFY_MESSAGE |
						  PSA_KEY_USAGE_EXPORT);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&attributes, IDENTITY_KEY_TYPE);
	psa_set_key_bits(&attributes, IDENTITY_ED25519_KEY_BITS);
	psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_PERSISTENT);
	psa_set_key_id(&attributes, IDENTITY_PERSISTENT_KEY_ID);

	status = psa_generate_key(&attributes, &key_id);
	psa_reset_key_attributes(&attributes);
	if (status != PSA_SUCCESS) {
		LOG_ERR("failed to generate persistent identity key (status=%d)", (int)status);
		return -EIO;
	}

	status = psa_export_public_key(key_id, public_key,
				       ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE,
				       &exported_length);
	if ((status != PSA_SUCCESS) ||
	    (exported_length != ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE)) {
		int reset_rc;

		LOG_ERR("failed to export generated public key (status=%d)", (int)status);
		reset_rc = identity_persistent_key_reset();
		if (reset_rc != 0) {
			LOG_ERR("failed to roll back generated identity key (rc=%d)", reset_rc);
		}
		rc = -EIO;
	} else {
		rc = 0;
	}

	status = psa_close_key(key_id);
	if (status != PSA_SUCCESS) {
		LOG_ERR("failed to close generated identity key (status=%d)", (int)status);
		rc = -EIO;
	}

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

	rc = identity_load_public_key(state.public_key);
	if (rc == -ENOENT) {
		rc = identity_generate_persistent_key(state.public_key);
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
	psa_key_id_t key_id = 0;
	psa_status_t status;
	size_t signature_length;
	int rc;

	if (((message == NULL) && (message_len > 0U)) || (signature == NULL) ||
	    (signature_len < ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE)) {
		return -EINVAL;
	}

	k_mutex_lock(&identity_lock, K_FOREVER);
	if (!state.initialized) {
		k_mutex_unlock(&identity_lock);
		return -EACCES;
	}

	status = psa_open_key(IDENTITY_PERSISTENT_KEY_ID, &key_id);
	if (status != PSA_SUCCESS) {
		rc = (status == PSA_ERROR_DOES_NOT_EXIST) ? -ENOENT : -EIO;
	} else {
		status = psa_sign_message(key_id, PSA_ALG_PURE_EDDSA, message, message_len,
					  signature, signature_len, &signature_length);
		if ((status != PSA_SUCCESS) ||
		    (signature_length != ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE)) {
			rc = -EIO;
		} else {
			rc = 0;
		}
		status = psa_close_key(key_id);
		if (status != PSA_SUCCESS) {
			LOG_ERR("failed to close persistent identity signing key (status=%d)",
				(int)status);
			rc = -EIO;
		}
	}

	if (rc != 0) {
		LOG_ERR("sign failed (rc=%d)", rc);
	}
	k_mutex_unlock(&identity_lock);
	return rc;
}

int altruist_identity_verify(const uint8_t *message, size_t message_len,
			     const uint8_t *signature, size_t signature_len)
{
	psa_key_id_t key_id = 0;
	psa_status_t status;
	int rc;

	if (((message == NULL) && (message_len > 0U)) || (signature == NULL) ||
	    (signature_len != ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE)) {
		return -EINVAL;
	}

	k_mutex_lock(&identity_lock, K_FOREVER);
	if (!state.initialized) {
		k_mutex_unlock(&identity_lock);
		return -EACCES;
	}

	status = psa_open_key(IDENTITY_PERSISTENT_KEY_ID, &key_id);
	if (status != PSA_SUCCESS) {
		rc = (status == PSA_ERROR_DOES_NOT_EXIST) ? -ENOENT : -EIO;
	} else {
		status = psa_verify_message(key_id, PSA_ALG_PURE_EDDSA, message, message_len,
					    signature, signature_len);
		if (status == PSA_SUCCESS) {
			rc = 0;
		} else if (status == PSA_ERROR_INVALID_SIGNATURE) {
			rc = -EINVAL;
		} else {
			rc = -EIO;
		}
		status = psa_close_key(key_id);
		if (status != PSA_SUCCESS) {
			LOG_ERR("failed to close persistent identity verification key (status=%d)",
				(int)status);
			rc = -EIO;
		}
	}

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
	rc = identity_generate_persistent_key(state.public_key);
	if (rc != 0) {
		LOG_ERR("identity reset failed to generate new keypair (rc=%d)", rc);
	}
	if (rc == 0) {
		state.initialized = true;
		LOG_INF("identity reset complete and key regenerated");
	} else {
		(void)memset(state.public_key, 0, sizeof(state.public_key));
		state.initialized = false;
	}

	k_mutex_unlock(&identity_lock);
	return rc;
}

#ifdef CONFIG_ZTEST
void altruist_identity_test_reset_state(void)
{
	k_mutex_lock(&identity_lock, K_FOREVER);
	(void)memset(state.public_key, 0, sizeof(state.public_key));
	state.initialized = false;
	k_mutex_unlock(&identity_lock);
}
#endif
