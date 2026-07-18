/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Altruist Zephyr firmware configuration contract.
 *
 * Portions of the key schema are adapted from:
 * airalab/altruist-firmware (esp32 branch),
 * config_manager/airrohr-cfg.h and config_manager/config_defaults.h.
 */

#ifndef ALTRUIST_CONFIG_H_
#define ALTRUIST_CONFIG_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALTRUIST_CONFIG_SCHEMA_VERSION 1U
#define ALTRUIST_CONFIG_LANG_MAX_LEN 3U
#define ALTRUIST_CONFIG_SSID_MAX_LEN 33U
#define ALTRUIST_CONFIG_PASSWORD_MAX_LEN 65U
#define ALTRUIST_CONFIG_OWNER_MAX_LEN 64U
#define ALTRUIST_CONFIG_URL_MAX_LEN 128U
#define ALTRUIST_CONFIG_HOST_POOL_MAX_LEN 256U
#define ALTRUIST_CONFIG_PRIVATE_KEY_MAX_LEN 129U

struct altruist_config {
	uint32_t schema_version;

	char current_lang[ALTRUIST_CONFIG_LANG_MAX_LEN];

	char wlanssid[ALTRUIST_CONFIG_SSID_MAX_LEN];
	char wlanpwd[ALTRUIST_CONFIG_PASSWORD_MAX_LEN];
	bool wlannopwd;

	char fs_ssid[ALTRUIST_CONFIG_SSID_MAX_LEN];
	char fs_pwd[ALTRUIST_CONFIG_PASSWORD_MAX_LEN];

	char rws_owner[ALTRUIST_CONFIG_OWNER_MAX_LEN];
	char robonomics_public_node[ALTRUIST_CONFIG_URL_MAX_LEN];
	char robonomics_connectivity_host[ALTRUIST_CONFIG_URL_MAX_LEN];
	char robonomics_connectivity_hosts[ALTRUIST_CONFIG_HOST_POOL_MAX_LEN];
	char private_key[ALTRUIST_CONFIG_PRIVATE_KEY_MAX_LEN];

	bool send2robonomics;
	bool send2csv;
	uint32_t sending_intervall_ms;
	uint32_t datalog_sending_intervall_ms;
	uint32_t sds_meas_interval_ms;
	uint32_t time_for_wifi_config;

	bool standalone;
};

typedef int (*altruist_config_migrate_cb_t)(uint32_t from_version,
						    struct altruist_config *cfg);

int altruist_config_init(void);
int altruist_config_load(void);
int altruist_config_save(void);
int altruist_config_reset_defaults(bool persist);
int altruist_config_replace(const struct altruist_config *new_cfg, bool persist);
int altruist_config_set_migration_hook(altruist_config_migrate_cb_t hook);
const struct altruist_config *altruist_config_get(void);

#ifdef __cplusplus
}
#endif

#endif /* ALTRUIST_CONFIG_H_ */
