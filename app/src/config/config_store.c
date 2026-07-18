/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Settings key schema adapted from:
 * airalab/altruist-firmware (esp32 branch),
 * config_manager/airrohr-cfg.h and config_manager/config_helpers.cpp.
 */

#include <altruist/config.h>

#include "config_defaults.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/settings/settings.h>
#include <zephyr/sys/printk.h>

#define CONFIG_ROOT "altruist"

#define CFG_KEY_SCHEMA_VERSION "schema_version"
#define CFG_KEY_CURRENT_LANG "current_lang"
#define CFG_KEY_WLANSSID "wlanssid"
#define CFG_KEY_WLANPWD "wlanpwd"
#define CFG_KEY_WLANNOPWD "wlannopwd"
#define CFG_KEY_FS_SSID "fs_ssid"
#define CFG_KEY_FS_PWD "fs_pwd"
#define CFG_KEY_RWS_OWNER "rws_owner"
#define CFG_KEY_ROBONOMICS_PUBLIC_NODE "robonomics_public_node"
#define CFG_KEY_ROBONOMICS_CONNECTIVITY_HOST "robonomics_connectivity_host"
#define CFG_KEY_ROBONOMICS_CONNECTIVITY_HOSTS "robonomics_connectivity_hosts"
#define CFG_KEY_PRIVATE_KEY "private_key"
#define CFG_KEY_SEND2ROBONOMICS "send2robonomics"
#define CFG_KEY_SEND2CSV "send2csv"
#define CFG_KEY_SENDING_INTERVALL_MS "sending_intervall_ms"
#define CFG_KEY_DATALOG_SENDING_INTERVALL_MS "datalog_sending_intervall_ms"
#define CFG_KEY_SDS_MEAS_INTERVAL_MS "sds_meas_interval_ms"
#define CFG_KEY_TIME_FOR_WIFI_CONFIG "time_for_wifi_config"
#define CFG_KEY_STANDALONE "standalone"

#define EXPORT_KEY(name) CONFIG_ROOT "/" name

static struct altruist_config g_cfg;
static altruist_config_migrate_cb_t g_migrate_hook;
static bool g_initialized;

static int cfg_read_string(size_t len, settings_read_cb read_cb, void *cb_arg,
			   char *dst, size_t dst_len)
{
	int rc;
	size_t read_len;

	if ((dst == NULL) || (dst_len == 0U)) {
		return -EINVAL;
	}

	read_len = (len < (dst_len - 1U)) ? len : (dst_len - 1U);
	rc = read_cb(cb_arg, dst, read_len);
	if (rc < 0) {
		return rc;
	}

	dst[read_len] = '\0';
	return 0;
}

static int cfg_read_bool(size_t len, settings_read_cb read_cb, void *cb_arg, bool *out)
{
	uint8_t raw = 0U;
	char text[6];
	int rc;

	if (out == NULL) {
		return -EINVAL;
	}

	if ((len == sizeof(bool)) || (len == sizeof(uint8_t))) {
		rc = read_cb(cb_arg, &raw, sizeof(raw));
		if (rc < 0) {
			return rc;
		}
		*out = (raw != 0U);
		return 0;
	}

	rc = cfg_read_string(len, read_cb, cb_arg, text, sizeof(text));
	if (rc < 0) {
		return rc;
	}

	if ((strcmp(text, "1") == 0) || (strcmp(text, "true") == 0) ||
	    (strcmp(text, "TRUE") == 0)) {
		*out = true;
	} else {
		*out = false;
	}

	return 0;
}

static int cfg_read_u32(size_t len, settings_read_cb read_cb, void *cb_arg, uint32_t *out)
{
	uint32_t raw = 0U;
	char text[12];
	int rc;

	if (out == NULL) {
		return -EINVAL;
	}

	if ((len == sizeof(uint32_t)) || (len == sizeof(unsigned int))) {
		rc = read_cb(cb_arg, &raw, sizeof(raw));
		if (rc < 0) {
			return rc;
		}
		*out = raw;
		return 0;
	}

	rc = cfg_read_string(len, read_cb, cb_arg, text, sizeof(text));
	if (rc < 0) {
		return rc;
	}

	*out = (uint32_t)strtoul(text, NULL, 10);
	return 0;
}

static int altruist_config_settings_set(const char *name, size_t len,
					settings_read_cb read_cb, void *cb_arg)
{
	if (settings_name_steq(name, CFG_KEY_SCHEMA_VERSION, NULL)) {
		return cfg_read_u32(len, read_cb, cb_arg, &g_cfg.schema_version);
	}
	if (settings_name_steq(name, CFG_KEY_CURRENT_LANG, NULL)) {
		return cfg_read_string(len, read_cb, cb_arg,
					       g_cfg.current_lang, sizeof(g_cfg.current_lang));
	}
	if (settings_name_steq(name, CFG_KEY_WLANSSID, NULL)) {
		return cfg_read_string(len, read_cb, cb_arg, g_cfg.wlanssid,
				       sizeof(g_cfg.wlanssid));
	}
	if (settings_name_steq(name, CFG_KEY_WLANPWD, NULL)) {
		return cfg_read_string(len, read_cb, cb_arg, g_cfg.wlanpwd,
				       sizeof(g_cfg.wlanpwd));
	}
	if (settings_name_steq(name, CFG_KEY_WLANNOPWD, NULL)) {
		return cfg_read_bool(len, read_cb, cb_arg, &g_cfg.wlannopwd);
	}
	if (settings_name_steq(name, CFG_KEY_FS_SSID, NULL)) {
		return cfg_read_string(len, read_cb, cb_arg, g_cfg.fs_ssid,
				       sizeof(g_cfg.fs_ssid));
	}
	if (settings_name_steq(name, CFG_KEY_FS_PWD, NULL)) {
		return cfg_read_string(len, read_cb, cb_arg, g_cfg.fs_pwd,
				       sizeof(g_cfg.fs_pwd));
	}
	if (settings_name_steq(name, CFG_KEY_RWS_OWNER, NULL)) {
		return cfg_read_string(len, read_cb, cb_arg, g_cfg.rws_owner,
				       sizeof(g_cfg.rws_owner));
	}
	if (settings_name_steq(name, CFG_KEY_ROBONOMICS_PUBLIC_NODE, NULL)) {
		return cfg_read_string(len, read_cb, cb_arg,
				       g_cfg.robonomics_public_node,
				       sizeof(g_cfg.robonomics_public_node));
	}
	if (settings_name_steq(name, CFG_KEY_ROBONOMICS_CONNECTIVITY_HOST, NULL)) {
		return cfg_read_string(len, read_cb, cb_arg,
				       g_cfg.robonomics_connectivity_host,
				       sizeof(g_cfg.robonomics_connectivity_host));
	}
	if (settings_name_steq(name, CFG_KEY_ROBONOMICS_CONNECTIVITY_HOSTS, NULL)) {
		return cfg_read_string(len, read_cb, cb_arg,
				       g_cfg.robonomics_connectivity_hosts,
				       sizeof(g_cfg.robonomics_connectivity_hosts));
	}
	if (settings_name_steq(name, CFG_KEY_PRIVATE_KEY, NULL)) {
		return cfg_read_string(len, read_cb, cb_arg, g_cfg.private_key,
				       sizeof(g_cfg.private_key));
	}
	if (settings_name_steq(name, CFG_KEY_SEND2ROBONOMICS, NULL)) {
		return cfg_read_bool(len, read_cb, cb_arg, &g_cfg.send2robonomics);
	}
	if (settings_name_steq(name, CFG_KEY_SEND2CSV, NULL)) {
		return cfg_read_bool(len, read_cb, cb_arg, &g_cfg.send2csv);
	}
	if (settings_name_steq(name, CFG_KEY_SENDING_INTERVALL_MS, NULL)) {
		return cfg_read_u32(len, read_cb, cb_arg, &g_cfg.sending_intervall_ms);
	}
	if (settings_name_steq(name, CFG_KEY_DATALOG_SENDING_INTERVALL_MS, NULL)) {
		return cfg_read_u32(len, read_cb, cb_arg,
				    &g_cfg.datalog_sending_intervall_ms);
	}
	if (settings_name_steq(name, CFG_KEY_SDS_MEAS_INTERVAL_MS, NULL)) {
		return cfg_read_u32(len, read_cb, cb_arg, &g_cfg.sds_meas_interval_ms);
	}
	if (settings_name_steq(name, CFG_KEY_TIME_FOR_WIFI_CONFIG, NULL)) {
		return cfg_read_u32(len, read_cb, cb_arg, &g_cfg.time_for_wifi_config);
	}
	if (settings_name_steq(name, CFG_KEY_STANDALONE, NULL)) {
		return cfg_read_bool(len, read_cb, cb_arg, &g_cfg.standalone);
	}

	return -ENOENT;
}

static int altruist_config_settings_get(const char *name, char *val, int val_len_max)
{
	if ((val == NULL) || (val_len_max <= 0)) {
		return -EINVAL;
	}

	if (settings_name_steq(name, CFG_KEY_SCHEMA_VERSION, NULL)) {
		if (val_len_max < (int)sizeof(g_cfg.schema_version)) {
			return -ENOMEM;
		}
		memcpy(val, &g_cfg.schema_version, sizeof(g_cfg.schema_version));
		return sizeof(g_cfg.schema_version);
	}
	if (settings_name_steq(name, CFG_KEY_CURRENT_LANG, NULL)) {
		return snprintk(val, val_len_max, "%s", g_cfg.current_lang);
	}
	if (settings_name_steq(name, CFG_KEY_WLANSSID, NULL)) {
		return snprintk(val, val_len_max, "%s", g_cfg.wlanssid);
	}
	if (settings_name_steq(name, CFG_KEY_WLANPWD, NULL)) {
		return snprintk(val, val_len_max, "%s", g_cfg.wlanpwd);
	}
	if (settings_name_steq(name, CFG_KEY_WLANNOPWD, NULL)) {
		if (val_len_max < (int)sizeof(g_cfg.wlannopwd)) {
			return -ENOMEM;
		}
		memcpy(val, &g_cfg.wlannopwd, sizeof(g_cfg.wlannopwd));
		return sizeof(g_cfg.wlannopwd);
	}
	if (settings_name_steq(name, CFG_KEY_FS_SSID, NULL)) {
		return snprintk(val, val_len_max, "%s", g_cfg.fs_ssid);
	}
	if (settings_name_steq(name, CFG_KEY_FS_PWD, NULL)) {
		return snprintk(val, val_len_max, "%s", g_cfg.fs_pwd);
	}
	if (settings_name_steq(name, CFG_KEY_RWS_OWNER, NULL)) {
		return snprintk(val, val_len_max, "%s", g_cfg.rws_owner);
	}
	if (settings_name_steq(name, CFG_KEY_ROBONOMICS_PUBLIC_NODE, NULL)) {
		return snprintk(val, val_len_max, "%s", g_cfg.robonomics_public_node);
	}
	if (settings_name_steq(name, CFG_KEY_ROBONOMICS_CONNECTIVITY_HOST, NULL)) {
		return snprintk(val, val_len_max, "%s", g_cfg.robonomics_connectivity_host);
	}
	if (settings_name_steq(name, CFG_KEY_ROBONOMICS_CONNECTIVITY_HOSTS, NULL)) {
		return snprintk(val, val_len_max, "%s", g_cfg.robonomics_connectivity_hosts);
	}
	if (settings_name_steq(name, CFG_KEY_PRIVATE_KEY, NULL)) {
		return snprintk(val, val_len_max, "%s", g_cfg.private_key);
	}
	if (settings_name_steq(name, CFG_KEY_SEND2ROBONOMICS, NULL)) {
		if (val_len_max < (int)sizeof(g_cfg.send2robonomics)) {
			return -ENOMEM;
		}
		memcpy(val, &g_cfg.send2robonomics, sizeof(g_cfg.send2robonomics));
		return sizeof(g_cfg.send2robonomics);
	}
	if (settings_name_steq(name, CFG_KEY_SEND2CSV, NULL)) {
		if (val_len_max < (int)sizeof(g_cfg.send2csv)) {
			return -ENOMEM;
		}
		memcpy(val, &g_cfg.send2csv, sizeof(g_cfg.send2csv));
		return sizeof(g_cfg.send2csv);
	}
	if (settings_name_steq(name, CFG_KEY_SENDING_INTERVALL_MS, NULL)) {
		if (val_len_max < (int)sizeof(g_cfg.sending_intervall_ms)) {
			return -ENOMEM;
		}
		memcpy(val, &g_cfg.sending_intervall_ms, sizeof(g_cfg.sending_intervall_ms));
		return sizeof(g_cfg.sending_intervall_ms);
	}
	if (settings_name_steq(name, CFG_KEY_DATALOG_SENDING_INTERVALL_MS, NULL)) {
		if (val_len_max < (int)sizeof(g_cfg.datalog_sending_intervall_ms)) {
			return -ENOMEM;
		}
		memcpy(val, &g_cfg.datalog_sending_intervall_ms,
		       sizeof(g_cfg.datalog_sending_intervall_ms));
		return sizeof(g_cfg.datalog_sending_intervall_ms);
	}
	if (settings_name_steq(name, CFG_KEY_SDS_MEAS_INTERVAL_MS, NULL)) {
		if (val_len_max < (int)sizeof(g_cfg.sds_meas_interval_ms)) {
			return -ENOMEM;
		}
		memcpy(val, &g_cfg.sds_meas_interval_ms, sizeof(g_cfg.sds_meas_interval_ms));
		return sizeof(g_cfg.sds_meas_interval_ms);
	}
	if (settings_name_steq(name, CFG_KEY_TIME_FOR_WIFI_CONFIG, NULL)) {
		if (val_len_max < (int)sizeof(g_cfg.time_for_wifi_config)) {
			return -ENOMEM;
		}
		memcpy(val, &g_cfg.time_for_wifi_config, sizeof(g_cfg.time_for_wifi_config));
		return sizeof(g_cfg.time_for_wifi_config);
	}
	if (settings_name_steq(name, CFG_KEY_STANDALONE, NULL)) {
		if (val_len_max < (int)sizeof(g_cfg.standalone)) {
			return -ENOMEM;
		}
		memcpy(val, &g_cfg.standalone, sizeof(g_cfg.standalone));
		return sizeof(g_cfg.standalone);
	}

	return -ENOENT;
}

static int altruist_config_settings_export(int (*cb)(const char *name,
						      const void *value,
						      size_t val_len))
{
	int rc;

	rc = cb(EXPORT_KEY(CFG_KEY_SCHEMA_VERSION), &g_cfg.schema_version,
		sizeof(g_cfg.schema_version));
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_CURRENT_LANG), g_cfg.current_lang,
		strlen(g_cfg.current_lang) + 1U);
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_WLANSSID), g_cfg.wlanssid,
		strlen(g_cfg.wlanssid) + 1U);
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_WLANPWD), g_cfg.wlanpwd,
		strlen(g_cfg.wlanpwd) + 1U);
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_WLANNOPWD), &g_cfg.wlannopwd,
		sizeof(g_cfg.wlannopwd));
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_FS_SSID), g_cfg.fs_ssid,
		strlen(g_cfg.fs_ssid) + 1U);
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_FS_PWD), g_cfg.fs_pwd,
		strlen(g_cfg.fs_pwd) + 1U);
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_RWS_OWNER), g_cfg.rws_owner,
		strlen(g_cfg.rws_owner) + 1U);
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_ROBONOMICS_PUBLIC_NODE),
		g_cfg.robonomics_public_node,
		strlen(g_cfg.robonomics_public_node) + 1U);
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_ROBONOMICS_CONNECTIVITY_HOST),
		g_cfg.robonomics_connectivity_host,
		strlen(g_cfg.robonomics_connectivity_host) + 1U);
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_ROBONOMICS_CONNECTIVITY_HOSTS),
		g_cfg.robonomics_connectivity_hosts,
		strlen(g_cfg.robonomics_connectivity_hosts) + 1U);
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_PRIVATE_KEY), g_cfg.private_key,
		strlen(g_cfg.private_key) + 1U);
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_SEND2ROBONOMICS), &g_cfg.send2robonomics,
		sizeof(g_cfg.send2robonomics));
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_SEND2CSV), &g_cfg.send2csv,
		sizeof(g_cfg.send2csv));
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_SENDING_INTERVALL_MS), &g_cfg.sending_intervall_ms,
		sizeof(g_cfg.sending_intervall_ms));
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_DATALOG_SENDING_INTERVALL_MS),
		&g_cfg.datalog_sending_intervall_ms,
		sizeof(g_cfg.datalog_sending_intervall_ms));
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_SDS_MEAS_INTERVAL_MS), &g_cfg.sds_meas_interval_ms,
		sizeof(g_cfg.sds_meas_interval_ms));
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_TIME_FOR_WIFI_CONFIG), &g_cfg.time_for_wifi_config,
		sizeof(g_cfg.time_for_wifi_config));
	if (rc) {
		return rc;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_STANDALONE), &g_cfg.standalone,
		sizeof(g_cfg.standalone));
	if (rc) {
		return rc;
	}

	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(altruist_config, CONFIG_ROOT,
			       altruist_config_settings_get,
			       altruist_config_settings_set,
			       NULL,
			       altruist_config_settings_export);

static int altruist_config_apply_migration(void)
{
	uint32_t from_version = g_cfg.schema_version;
	int rc;

	if (from_version >= ALTRUIST_CONFIG_SCHEMA_VERSION) {
		return 0;
	}

	if (g_migrate_hook != NULL) {
		rc = g_migrate_hook(from_version, &g_cfg);
		if (rc < 0) {
			return rc;
		}
	}

	g_cfg.schema_version = ALTRUIST_CONFIG_SCHEMA_VERSION;
	return altruist_config_save();
}

int altruist_config_init(void)
{
	int rc;

	altruist_config_apply_defaults(&g_cfg);

	rc = settings_subsys_init();
	if ((rc < 0) && (rc != -EALREADY)) {
		return rc;
	}

	rc = altruist_config_load();
	if ((rc < 0) && (rc != -ENOENT)) {
		return rc;
	}

	rc = altruist_config_apply_migration();
	if (rc < 0) {
		return rc;
	}

	g_initialized = true;
	return 0;
}

int altruist_config_load(void)
{
	return settings_load_subtree(CONFIG_ROOT);
}

int altruist_config_save(void)
{
	return settings_save();
}

int altruist_config_reset_defaults(bool persist)
{
	altruist_config_apply_defaults(&g_cfg);
	if (!persist) {
		return 0;
	}

	return altruist_config_save();
}

int altruist_config_replace(const struct altruist_config *new_cfg, bool persist)
{
	if (new_cfg == NULL) {
		return -EINVAL;
	}

	memcpy(&g_cfg, new_cfg, sizeof(g_cfg));
	if (!persist) {
		return 0;
	}

	return altruist_config_save();
}

int altruist_config_set_migration_hook(altruist_config_migrate_cb_t hook)
{
	g_migrate_hook = hook;
	return 0;
}

const struct altruist_config *altruist_config_get(void)
{
	if (!g_initialized) {
		altruist_config_apply_defaults(&g_cfg);
	}

	return &g_cfg;
}
