/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Defaults adapted from:
 * airalab/altruist-firmware (esp32 branch),
 * config_manager/config_defaults.h.
 */

#include "config_defaults.h"

#include <string.h>

void altruist_config_apply_defaults(struct altruist_config *cfg)
{
	if (cfg == NULL) {
		return;
	}

	memset(cfg, 0, sizeof(*cfg));

	cfg->schema_version = ALTRUIST_CONFIG_SCHEMA_VERSION;

	strncpy(cfg->current_lang, "en", sizeof(cfg->current_lang) - 1);

	strncpy(cfg->wlanssid, "Not Set", sizeof(cfg->wlanssid) - 1);
	cfg->wlanpwd[0] = '\0';
	cfg->wlannopwd = false;

	strncpy(cfg->fs_ssid, "Altruist", sizeof(cfg->fs_ssid) - 1);
	cfg->fs_pwd[0] = '\0';

	strncpy(cfg->rws_owner, "Not Set", sizeof(cfg->rws_owner) - 1);
	cfg->robonomics_public_node[0] = '\0';
	cfg->robonomics_connectivity_host[0] = '\0';
	cfg->robonomics_connectivity_hosts[0] = '\0';
	cfg->private_key[0] = '\0';

	cfg->send2robonomics = false;
	cfg->send2csv = false;
	cfg->sending_interval_ms = 145000U;
	cfg->datalog_sending_interval_ms = 600000U;
	cfg->sds_meas_interval_ms = 10000U;
	cfg->time_for_wifi_config = 600000U;

	cfg->standalone = false;
}
