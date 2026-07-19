/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <altruist/identity.h>

#include <zephyr/logging/log.h>

#include <psa/crypto.h>
#include <zephyr/psa/key_ids.h>

LOG_MODULE_REGISTER(crypto_identity);

#define IDENTITY_KEY_ID ZEPHYR_PSA_APPLICATION_KEY_ID_RANGE_BEGIN
/* PSA Crypto expects 255 for Ed25519 key-bit attributes (curve parameter size). */
#define IDENTITY_ED25519_KEY_BITS 255
#define IDENTITY_KEY_TYPE PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS)

static int identity_generate_key()
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key_id;
	psa_status_t ret;

	/* Configure key attributes for Ed25519 */
    psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_PERSISTENT);
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_VERIFY_MESSAGE |
						  PSA_KEY_USAGE_EXPORT);
	psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
    psa_set_key_id(&key_attributes, IDENTITY_PERSISTENT_KEY_ID);
	psa_set_key_type(&attributes, IDENTITY_KEY_TYPE); 
	psa_set_key_bits(&attributes, IDENTITY_ED25519_KEY_BITS);

	ret = psa_generate_key(&attributes, &key_id);
	if (ret != PSA_SUCCESS) {
        LOG_ERR("Failed to generate the key. (%d)", ret);
		return -EIO;
	}

    /* Purge the key from volatile memory. Has the same affect than resetting the device. */
    ret = psa_purge_key(IDENTITY_KEY_ID);
    if (ret != PSA_SUCCESS) {
        LOG_ERR("Failed to purge the generated key from volatile memory. (%d).", ret);
        return -EIO;
    }

    LOG_INF("Persistent key generated.");
    return 0;
}

int altruist_identity_sign(
    const uint8_t *message,
    size_t message_len,
    uint8_t *signature,
    size_t signature_len,
) {
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
	psa_destroy_key(key_id);
	psa_reset_key_attributes(&attributes);

	return rc;
}

int altruist_identity_verify(const uint8_t *public_key, size_t public_key_len,
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
	psa_destroy_key(key_id);
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
		printk("altruist_identity: PSA Crypto initialization failed with status %d\n",
		       (int)status);
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
