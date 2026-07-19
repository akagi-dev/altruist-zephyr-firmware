#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>

#include <altruist/config_wifi_credentials.h>
#include <altruist/wifi_manager.h>

static struct wifi_manager_credentials stored_credentials;
static bool stored_credentials_valid;
static bool settings_ready;
K_MUTEX_DEFINE(wifi_credentials_lock);

static bool wifi_credentials_is_null_terminated(const char *value, size_t value_len)
{
	if (value == NULL || value_len == 0U) {
		return false;
	}

	return memchr(value, '\0', value_len) != NULL;
}

static int wifi_credentials_settings_set(const char *name, size_t len_rd, settings_read_cb read_cb,
					 void *cb_arg)
{
	struct wifi_manager_credentials credentials = { 0 };
	ssize_t len;

	if (strcmp(name, "credentials") != 0) {
		return -ENOENT;
	}

	if (len_rd != sizeof(credentials)) {
		return -EINVAL;
	}

	len = read_cb(cb_arg, &credentials, sizeof(credentials));
	if (len < 0) {
		return (int)len;
	}

	if ((size_t)len != sizeof(credentials)) {
		return -EINVAL;
	}

	if (!wifi_credentials_is_null_terminated(credentials.ssid, sizeof(credentials.ssid)) ||
	    !wifi_credentials_is_null_terminated(credentials.psk, sizeof(credentials.psk))) {
		memset(&stored_credentials, 0, sizeof(stored_credentials));
		stored_credentials_valid = false;
		return -EINVAL;
	}

	if (credentials.ssid[0] == '\0') {
		memset(&stored_credentials, 0, sizeof(stored_credentials));
		stored_credentials_valid = false;
		return 0;
	}

	stored_credentials = credentials;
	stored_credentials_valid = true;
	return 0;
}

static struct settings_handler wifi_credentials_settings = {
	.name = "altruist/wifi",
	.h_set = wifi_credentials_settings_set,
};

static int wifi_credentials_storage_init_locked(void)
{
	int rc;

	if (settings_ready) {
		return 0;
	}

	rc = settings_subsys_init();
	if (rc != 0) {
		return rc;
	}

	rc = settings_register(&wifi_credentials_settings);
	if (rc != 0) {
		return rc;
	}

	rc = settings_load_subtree("altruist/wifi");
	if (rc != 0) {
		return rc;
	}

	settings_ready = true;
	return 0;
}

bool altruist_config_get_wifi_credentials(struct wifi_manager_credentials *out)
{
	int rc;

	if (out == NULL) {
		return false;
	}

	k_mutex_lock(&wifi_credentials_lock, K_FOREVER);
	rc = wifi_credentials_storage_init_locked();
	if (rc != 0 || !stored_credentials_valid) {
		k_mutex_unlock(&wifi_credentials_lock);
		return false;
	}

	*out = stored_credentials;
	k_mutex_unlock(&wifi_credentials_lock);
	return true;
}

int altruist_config_set_wifi_credentials(const struct wifi_manager_credentials *credentials)
{
	int rc;
	struct wifi_manager_credentials saved_credentials;

	if (credentials == NULL) {
		return -EINVAL;
	}

	if (!wifi_credentials_is_null_terminated(credentials->ssid, sizeof(credentials->ssid)) ||
	    !wifi_credentials_is_null_terminated(credentials->psk, sizeof(credentials->psk))) {
		return -EINVAL;
	}

	saved_credentials = *credentials;

	if (saved_credentials.ssid[0] == '\0') {
		return -EINVAL;
	}

	k_mutex_lock(&wifi_credentials_lock, K_FOREVER);
	rc = wifi_credentials_storage_init_locked();
	if (rc == 0) {
		rc = settings_save_one("altruist/wifi/credentials", &saved_credentials,
				       sizeof(saved_credentials));
	}
	if (rc == 0) {
		stored_credentials = saved_credentials;
		stored_credentials_valid = true;
	}
	k_mutex_unlock(&wifi_credentials_lock);

	return rc;
}

int altruist_config_clear_wifi_credentials(void)
{
	int rc;

	k_mutex_lock(&wifi_credentials_lock, K_FOREVER);
	rc = wifi_credentials_storage_init_locked();
	if (rc == 0) {
		rc = settings_delete("altruist/wifi/credentials");
	}
	if (rc == 0) {
		memset(&stored_credentials, 0, sizeof(stored_credentials));
		stored_credentials_valid = false;
	}
	k_mutex_unlock(&wifi_credentials_lock);

	return rc;
}
