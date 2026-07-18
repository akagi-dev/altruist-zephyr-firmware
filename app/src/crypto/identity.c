/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <altruist/identity.h>

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/printk.h>
#include <zephyr/toolchain.h>

#include "monocypher/monocypher-ed25519.h"

#define IDENTITY_SETTINGS_SUBTREE "altruist/identity"
#define IDENTITY_SETTINGS_PRIVATE_KEY IDENTITY_SETTINGS_SUBTREE "/private_key"
#define IDENTITY_SETTINGS_PUBLIC_KEY IDENTITY_SETTINGS_SUBTREE "/public_key"

/* Monocypher's Ed25519 secret key is the 32-byte seed followed by the 32-byte
 * public key. The persisted private key is only the seed; the matching public
 * key half is recomputed on demand.
 */
#define IDENTITY_ED25519_SECRET_KEY_SIZE 64U

struct identity_state {
	bool initialized;
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

static int identity_generate_keypair(uint8_t *private_key, uint8_t *public_key)
{
	uint8_t seed[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE];
	uint8_t secret_key[IDENTITY_ED25519_SECRET_KEY_SIZE];
	int rc;

	if ((private_key == NULL) || (public_key == NULL)) {
		return -EINVAL;
	}

	rc = sys_csrand_get(seed, sizeof(seed));
	if (rc != 0) {
		return rc;
	}

	/* crypto_ed25519_key_pair() wipes the seed it is given, so keep an
	 * authoritative copy in the private key output first.
	 */
	memcpy(private_key, seed, sizeof(seed));
	crypto_ed25519_key_pair(secret_key, public_key, seed);

	crypto_wipe(secret_key, sizeof(secret_key));
	crypto_wipe(seed, sizeof(seed));

	return 0;
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
		if (bytes_read != (ssize_t)len) {
			ctx->read_error = (bytes_read < 0) ? (int)bytes_read : -EIO;
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
		if (bytes_read != (ssize_t)len) {
			ctx->read_error = (bytes_read < 0) ? (int)bytes_read : -EIO;
			return 0;
		}

		ctx->has_public_key = true;
		return 0;
	}

	return 0;
}

/*
 * Weak linkage allows tests to provide strong symbol overrides for these storage
 * hooks, so unit tests can validate identity lifecycle without a Zephyr Settings
 * backend and without touching persistent flash state.
 */
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

/* See altruist_identity_storage_load() note about weak test overrides. */
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

/* See altruist_identity_storage_load() note about weak test overrides. */
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
	uint8_t seed[ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE];
	uint8_t secret_key[IDENTITY_ED25519_SECRET_KEY_SIZE];
	uint8_t public_key[ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE];

	if ((private_key == NULL) ||
	    (private_key_len != ALTRUIST_IDENTITY_ED25519_PRIVATE_KEY_SIZE) ||
	    ((message == NULL) && (message_len > 0U)) || (signature == NULL) ||
	    (signature_len < ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE)) {
		return -EINVAL;
	}

	/* Reconstruct the full Ed25519 secret key (seed || public key) from the
	 * persisted seed. crypto_ed25519_key_pair() wipes the seed it receives.
	 */
	memcpy(seed, private_key, sizeof(seed));
	crypto_ed25519_key_pair(secret_key, public_key, seed);

	crypto_ed25519_sign(signature, secret_key, message, message_len);

	crypto_wipe(secret_key, sizeof(secret_key));
	crypto_wipe(seed, sizeof(seed));

	return 0;
}

int altruist_identity_verify_detached(const uint8_t *public_key, size_t public_key_len,
				      const uint8_t *message, size_t message_len,
				      const uint8_t *signature, size_t signature_len)
{
	if ((public_key == NULL) ||
	    (public_key_len != ALTRUIST_IDENTITY_ED25519_PUBLIC_KEY_SIZE) ||
	    ((message == NULL) && (message_len > 0U)) || (signature == NULL) ||
	    (signature_len != ALTRUIST_IDENTITY_ED25519_SIGNATURE_SIZE)) {
		return -EINVAL;
	}

	if (crypto_ed25519_check(signature, public_key, message, message_len) != 0) {
		return -EINVAL;
	}

	return 0;
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
		printk("altruist_identity: Settings subsystem initialization failed with error code %d. "
		       "Verify Settings backend configuration and storage availability.\n", rc);
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
		crypto_wipe(state.private_key, sizeof(state.private_key));
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
	crypto_wipe(state.private_key, sizeof(state.private_key));
	(void)memset(state.public_key, 0, sizeof(state.public_key));
	state.initialized = false;
	k_mutex_unlock(&identity_lock);
}
#endif
