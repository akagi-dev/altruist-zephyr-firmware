/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <altruist/identity.h>

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <psa/crypto.h>

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/toolchain.h>

#define IDENTITY_SETTINGS_SUBTREE "altruist/identity"
#define IDENTITY_SETTINGS_PRIVATE_KEY IDENTITY_SETTINGS_SUBTREE "/private_key"
#define IDENTITY_SETTINGS_PUBLIC_KEY IDENTITY_SETTINGS_SUBTREE "/public_key"

struct identity_state {
	bool initialized;
	bool psa_initialized;
	uint8_t private_key[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE];
	uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];
};

struct identity_settings_load_ctx {
	uint8_t *private_key;
	uint8_t *public_key;
	bool has_private_key;
	bool has_public_key;
	int read_error;
};

static struct identity_state state;
K_MUTEX_DEFINE(identity_lock);

static int identity_psa_status_to_errno(psa_status_t status)
{
	switch (status) {
	case PSA_SUCCESS:
		return 0;
	case PSA_ERROR_NOT_SUPPORTED:
		return -ENOTSUP;
	case PSA_ERROR_INVALID_ARGUMENT:
	case PSA_ERROR_INVALID_HANDLE:
	case PSA_ERROR_INVALID_SIGNATURE:
		return -EINVAL;
	case PSA_ERROR_BUFFER_TOO_SMALL:
		return -EMSGSIZE;
	case PSA_ERROR_INSUFFICIENT_MEMORY:
	case PSA_ERROR_INSUFFICIENT_STORAGE:
		return -ENOMEM;
	case PSA_ERROR_BAD_STATE:
		return -EAGAIN;
	case PSA_ERROR_NOT_PERMITTED:
		return -EACCES;
	case PSA_ERROR_DOES_NOT_EXIST:
		return -ENOENT;
	default:
		return -EIO;
	}
}

static int identity_psa_init_once(void)
{
	int rc = 0;

	if (!state.psa_initialized) {
		psa_status_t status = psa_crypto_init();

		rc = identity_psa_status_to_errno(status);
		if (rc == 0) {
			state.psa_initialized = true;
		}
	}

	return rc;
}

static int identity_import_private_key_for_sign(const uint8_t *private_key, size_t private_key_len,
						psa_key_id_t *key_id)
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_status_t status;

	if ((private_key == NULL) || (key_id == NULL) ||
	    (private_key_len != ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE)) {
		return -EINVAL;
	}

	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attributes, 255);
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);

	status = psa_import_key(&attributes, private_key, private_key_len, key_id);
	psa_reset_key_attributes(&attributes);

	return identity_psa_status_to_errno(status);
}

static int identity_import_public_key_for_verify(const uint8_t *public_key, size_t public_key_len,
						 psa_key_id_t *key_id)
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_status_t status;

	if ((public_key == NULL) || (key_id == NULL) ||
	    (public_key_len != ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE)) {
		return -EINVAL;
	}

	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attributes, 255);
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_MESSAGE);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);

	status = psa_import_key(&attributes, public_key, public_key_len, key_id);
	psa_reset_key_attributes(&attributes);

	return identity_psa_status_to_errno(status);
}

static int identity_generate_keypair(uint8_t *private_key, uint8_t *public_key)
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id = PSA_KEY_ID_NULL;
	psa_status_t status;
	size_t out_len = 0U;
	int rc;

	if ((private_key == NULL) || (public_key == NULL)) {
		return -EINVAL;
	}

	rc = identity_psa_init_once();
	if (rc != 0) {
		return rc;
	}

	psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&attributes, 255);
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_EXPORT | PSA_KEY_USAGE_SIGN_MESSAGE);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);

	status = psa_generate_key(&attributes, &key_id);
	psa_reset_key_attributes(&attributes);
	if (status != PSA_SUCCESS) {
		return identity_psa_status_to_errno(status);
	}

	status = psa_export_key(key_id, private_key, ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE, &out_len);
	if ((status != PSA_SUCCESS) ||
	    (out_len != ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE)) {
		rc = (status == PSA_SUCCESS) ? -EIO : identity_psa_status_to_errno(status);
		(void)psa_destroy_key(key_id);
		return rc;
	}

	status = psa_export_public_key(key_id, public_key, ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE,
				       &out_len);
	if ((status != PSA_SUCCESS) ||
	    (out_len != ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE)) {
		rc = (status == PSA_SUCCESS) ? -EIO : identity_psa_status_to_errno(status);
		(void)psa_destroy_key(key_id);
		return rc;
	}

	status = psa_destroy_key(key_id);
	return identity_psa_status_to_errno(status);
}

static int identity_settings_load_cb(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg,
				     void *param)
{
	struct identity_settings_load_ctx *ctx = param;
	ssize_t bytes_read;

	if ((key == NULL) || (read_cb == NULL) || (ctx == NULL)) {
		return -EINVAL;
	}

	if (strcmp(key, "private_key") == 0) {
		if (len != ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE) {
			ctx->read_error = -EINVAL;
			return 0;
		}

		bytes_read = read_cb(cb_arg, ctx->private_key, len);
		if (bytes_read < 0) {
			ctx->read_error = (int)bytes_read;
			return 0;
		}

		ctx->has_private_key = true;
		return 0;
	}

	if (strcmp(key, "public_key") == 0) {
		if (len != ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE) {
			ctx->read_error = -EINVAL;
			return 0;
		}

		bytes_read = read_cb(cb_arg, ctx->public_key, len);
		if (bytes_read < 0) {
			ctx->read_error = (int)bytes_read;
			return 0;
		}

		ctx->has_public_key = true;
		return 0;
	}

	return 0;
}

int __weak altruist_identity_storage_load(uint8_t private_key[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE],
					  uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE])
{
	struct identity_settings_load_ctx ctx = {
		.private_key = private_key,
		.public_key = public_key,
		.has_private_key = false,
		.has_public_key = false,
		.read_error = 0,
	};

	int rc;

	if ((private_key == NULL) || (public_key == NULL)) {
		return -EINVAL;
	}

	rc = settings_load_subtree_direct(IDENTITY_SETTINGS_SUBTREE, identity_settings_load_cb, &ctx);
	if (rc != 0) {
		return rc;
	}

	if (ctx.read_error != 0) {
		return ctx.read_error;
	}

	if (!ctx.has_private_key || !ctx.has_public_key) {
		return -ENOENT;
	}

	return 0;
}

int __weak altruist_identity_storage_save(
	const uint8_t private_key[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE],
	const uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE])
{
	int rc;

	if ((private_key == NULL) || (public_key == NULL)) {
		return -EINVAL;
	}

	rc = settings_save_one(IDENTITY_SETTINGS_PRIVATE_KEY, private_key,
			       ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE);
	if (rc != 0) {
		return rc;
	}

	return settings_save_one(IDENTITY_SETTINGS_PUBLIC_KEY, public_key,
				 ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE);
}

int __weak altruist_identity_storage_reset(void)
{
	int rc_private = settings_delete(IDENTITY_SETTINGS_PRIVATE_KEY);
	int rc_public = settings_delete(IDENTITY_SETTINGS_PUBLIC_KEY);

	if ((rc_private != 0) && (rc_private != -ENOENT)) {
		return rc_private;
	}

	if ((rc_public != 0) && (rc_public != -ENOENT)) {
		return rc_public;
	}

	return 0;
}

int altruist_identity_sign_detached(const uint8_t *private_key, size_t private_key_len,
				    const uint8_t *message, size_t message_len,
				    uint8_t *signature, size_t signature_len)
{
	psa_key_id_t key_id = PSA_KEY_ID_NULL;
	size_t actual_signature_len = 0U;
	psa_status_t status;
	int rc;

	if (((message == NULL) && (message_len > 0U)) || (signature == NULL) ||
	    (signature_len < ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE)) {
		return -EINVAL;
	}

	rc = identity_psa_init_once();
	if (rc != 0) {
		return rc;
	}

	rc = identity_import_private_key_for_sign(private_key, private_key_len, &key_id);
	if (rc != 0) {
		return rc;
	}

	status = psa_sign_message(key_id, PSA_ALG_PURE_EDDSA, message, message_len, signature,
				  signature_len, &actual_signature_len);
	(void)psa_destroy_key(key_id);
	if (status != PSA_SUCCESS) {
		return identity_psa_status_to_errno(status);
	}

	if (actual_signature_len != ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE) {
		return -EIO;
	}

	return 0;
}

int altruist_identity_verify_detached(const uint8_t *public_key, size_t public_key_len,
				      const uint8_t *message, size_t message_len,
				      const uint8_t *signature, size_t signature_len)
{
	psa_key_id_t key_id = PSA_KEY_ID_NULL;
	psa_status_t status;
	int rc;

	if (((message == NULL) && (message_len > 0U)) || (signature == NULL) ||
	    (signature_len != ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE)) {
		return -EINVAL;
	}

	rc = identity_psa_init_once();
	if (rc != 0) {
		return rc;
	}

	rc = identity_import_public_key_for_verify(public_key, public_key_len, &key_id);
	if (rc != 0) {
		return rc;
	}

	status = psa_verify_message(key_id, PSA_ALG_PURE_EDDSA, message, message_len, signature,
				    signature_len);
	(void)psa_destroy_key(key_id);
	return identity_psa_status_to_errno(status);
}

int altruist_identity_init(void)
{
	int rc;

	k_mutex_lock(&identity_lock, K_FOREVER);

	if (state.initialized) {
		k_mutex_unlock(&identity_lock);
		return 0;
	}

	rc = settings_subsys_init();
	if ((rc != 0) && (rc != -EALREADY)) {
		k_mutex_unlock(&identity_lock);
		return rc;
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
	k_mutex_unlock(&identity_lock);
	return rc;
}

int altruist_identity_reset(void)
{
	int rc;

	k_mutex_lock(&identity_lock, K_FOREVER);
	rc = altruist_identity_storage_reset();
	if (rc == 0) {
		(void)memset(state.private_key, 0, sizeof(state.private_key));
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
	(void)memset(state.private_key, 0, sizeof(state.private_key));
	(void)memset(state.public_key, 0, sizeof(state.public_key));
	state.initialized = false;
	k_mutex_unlock(&identity_lock);
}
#endif
