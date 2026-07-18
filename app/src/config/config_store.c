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
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
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
#define CFG_KEY_SENDING_INTERVAL_MS "sending_interval_ms"
#define CFG_KEY_DATALOG_SENDING_INTERVAL_MS "datalog_sending_interval_ms"
#define CFG_KEY_SENDING_INTERVAL_MS_LEGACY "sending_intervall_ms"
#define CFG_KEY_DATALOG_SENDING_INTERVAL_MS_LEGACY "datalog_sending_intervall_ms"
#define CFG_KEY_SDS_MEAS_INTERVAL_MS "sds_meas_interval_ms"
#define CFG_KEY_TIME_FOR_WIFI_CONFIG "time_for_wifi_config"
#define CFG_KEY_STANDALONE "standalone"

#define EXPORT_KEY(name) CONFIG_ROOT "/" name

static struct altruist_config g_cfg;
static struct altruist_config g_cfg_snapshot;
static altruist_config_migrate_cb_t g_migrate_hook;
static bool g_initialized;
K_MUTEX_DEFINE(g_cfg_lock);

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
	unsigned long parsed;
	uint32_t raw = 0U;
	char text[12];
	char *endptr;
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

	errno = 0;
	parsed = strtoul(text, &endptr, 10);
	if ((endptr == text) || (*endptr != '\0') || (errno == ERANGE) || (parsed > UINT32_MAX)) {
		return -EINVAL;
	}

	*out = (uint32_t)parsed;
	return 0;
}

static int cfg_write_bool(char *val, int val_len_max, bool src)
{
	uint8_t raw = src ? 1U : 0U;

	if (val_len_max < (int)sizeof(raw)) {
		return -ENOMEM;
	}

	memcpy(val, &raw, sizeof(raw));
	return sizeof(raw);
}

static int altruist_config_settings_set(const char *name, size_t len,
					settings_read_cb read_cb, void *cb_arg)
{
	int rc;

	k_mutex_lock(&g_cfg_lock, K_FOREVER);

	if (settings_name_steq(name, CFG_KEY_SCHEMA_VERSION, NULL)) {
		rc = cfg_read_u32(len, read_cb, cb_arg, &g_cfg.schema_version);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_CURRENT_LANG, NULL)) {
		rc = cfg_read_string(len, read_cb, cb_arg,
				     g_cfg.current_lang, sizeof(g_cfg.current_lang));
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_WLANSSID, NULL)) {
		rc = cfg_read_string(len, read_cb, cb_arg, g_cfg.wlanssid,
				     sizeof(g_cfg.wlanssid));
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_WLANPWD, NULL)) {
		rc = cfg_read_string(len, read_cb, cb_arg, g_cfg.wlanpwd,
				     sizeof(g_cfg.wlanpwd));
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_WLANNOPWD, NULL)) {
		rc = cfg_read_bool(len, read_cb, cb_arg, &g_cfg.wlannopwd);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_FS_SSID, NULL)) {
		rc = cfg_read_string(len, read_cb, cb_arg, g_cfg.fs_ssid,
				     sizeof(g_cfg.fs_ssid));
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_FS_PWD, NULL)) {
		rc = cfg_read_string(len, read_cb, cb_arg, g_cfg.fs_pwd,
				     sizeof(g_cfg.fs_pwd));
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_RWS_OWNER, NULL)) {
		rc = cfg_read_string(len, read_cb, cb_arg, g_cfg.rws_owner,
				     sizeof(g_cfg.rws_owner));
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_ROBONOMICS_PUBLIC_NODE, NULL)) {
		rc = cfg_read_string(len, read_cb, cb_arg,
				     g_cfg.robonomics_public_node,
				     sizeof(g_cfg.robonomics_public_node));
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_ROBONOMICS_CONNECTIVITY_HOST, NULL)) {
		rc = cfg_read_string(len, read_cb, cb_arg,
				     g_cfg.robonomics_connectivity_host,
				     sizeof(g_cfg.robonomics_connectivity_host));
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_ROBONOMICS_CONNECTIVITY_HOSTS, NULL)) {
		rc = cfg_read_string(len, read_cb, cb_arg,
				     g_cfg.robonomics_connectivity_hosts,
				     sizeof(g_cfg.robonomics_connectivity_hosts));
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_PRIVATE_KEY, NULL)) {
		rc = cfg_read_string(len, read_cb, cb_arg, g_cfg.private_key,
				     sizeof(g_cfg.private_key));
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_SEND2ROBONOMICS, NULL)) {
		rc = cfg_read_bool(len, read_cb, cb_arg, &g_cfg.send2robonomics);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_SEND2CSV, NULL)) {
		rc = cfg_read_bool(len, read_cb, cb_arg, &g_cfg.send2csv);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_SENDING_INTERVAL_MS, NULL) ||
	    settings_name_steq(name, CFG_KEY_SENDING_INTERVAL_MS_LEGACY, NULL)) {
		rc = cfg_read_u32(len, read_cb, cb_arg, &g_cfg.sending_interval_ms);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_DATALOG_SENDING_INTERVAL_MS, NULL) ||
	    settings_name_steq(name, CFG_KEY_DATALOG_SENDING_INTERVAL_MS_LEGACY, NULL)) {
		rc = cfg_read_u32(len, read_cb, cb_arg,
				  &g_cfg.datalog_sending_interval_ms);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_SDS_MEAS_INTERVAL_MS, NULL)) {
		rc = cfg_read_u32(len, read_cb, cb_arg, &g_cfg.sds_meas_interval_ms);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_TIME_FOR_WIFI_CONFIG, NULL)) {
		rc = cfg_read_u32(len, read_cb, cb_arg, &g_cfg.time_for_wifi_config);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_STANDALONE, NULL)) {
		rc = cfg_read_bool(len, read_cb, cb_arg, &g_cfg.standalone);
		goto out;
	}

	rc = -ENOENT;
out:
	k_mutex_unlock(&g_cfg_lock);
	return rc;
}

static int altruist_config_settings_get(const char *name, char *val, int val_len_max)
{
	int rc;

	if ((val == NULL) || (val_len_max <= 0)) {
		return -EINVAL;
	}

	k_mutex_lock(&g_cfg_lock, K_FOREVER);

	if (settings_name_steq(name, CFG_KEY_SCHEMA_VERSION, NULL)) {
		if (val_len_max < (int)sizeof(g_cfg.schema_version)) {
			rc = -ENOMEM;
			goto out;
		}
		memcpy(val, &g_cfg.schema_version, sizeof(g_cfg.schema_version));
		rc = sizeof(g_cfg.schema_version);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_CURRENT_LANG, NULL)) {
		rc = snprintk(val, val_len_max, "%s", g_cfg.current_lang);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_WLANSSID, NULL)) {
		rc = snprintk(val, val_len_max, "%s", g_cfg.wlanssid);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_WLANPWD, NULL)) {
		rc = snprintk(val, val_len_max, "%s", g_cfg.wlanpwd);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_WLANNOPWD, NULL)) {
		rc = cfg_write_bool(val, val_len_max, g_cfg.wlannopwd);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_FS_SSID, NULL)) {
		rc = snprintk(val, val_len_max, "%s", g_cfg.fs_ssid);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_FS_PWD, NULL)) {
		rc = snprintk(val, val_len_max, "%s", g_cfg.fs_pwd);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_RWS_OWNER, NULL)) {
		rc = snprintk(val, val_len_max, "%s", g_cfg.rws_owner);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_ROBONOMICS_PUBLIC_NODE, NULL)) {
		rc = snprintk(val, val_len_max, "%s", g_cfg.robonomics_public_node);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_ROBONOMICS_CONNECTIVITY_HOST, NULL)) {
		rc = snprintk(val, val_len_max, "%s", g_cfg.robonomics_connectivity_host);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_ROBONOMICS_CONNECTIVITY_HOSTS, NULL)) {
		rc = snprintk(val, val_len_max, "%s", g_cfg.robonomics_connectivity_hosts);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_PRIVATE_KEY, NULL)) {
		rc = snprintk(val, val_len_max, "%s", g_cfg.private_key);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_SEND2ROBONOMICS, NULL)) {
		rc = cfg_write_bool(val, val_len_max, g_cfg.send2robonomics);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_SEND2CSV, NULL)) {
		rc = cfg_write_bool(val, val_len_max, g_cfg.send2csv);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_SENDING_INTERVAL_MS, NULL)) {
		if (val_len_max < (int)sizeof(g_cfg.sending_interval_ms)) {
			rc = -ENOMEM;
			goto out;
		}
		memcpy(val, &g_cfg.sending_interval_ms, sizeof(g_cfg.sending_interval_ms));
		rc = sizeof(g_cfg.sending_interval_ms);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_DATALOG_SENDING_INTERVAL_MS, NULL)) {
		if (val_len_max < (int)sizeof(g_cfg.datalog_sending_interval_ms)) {
			rc = -ENOMEM;
			goto out;
		}
		memcpy(val, &g_cfg.datalog_sending_interval_ms,
		       sizeof(g_cfg.datalog_sending_interval_ms));
		rc = sizeof(g_cfg.datalog_sending_interval_ms);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_SDS_MEAS_INTERVAL_MS, NULL)) {
		if (val_len_max < (int)sizeof(g_cfg.sds_meas_interval_ms)) {
			rc = -ENOMEM;
			goto out;
		}
		memcpy(val, &g_cfg.sds_meas_interval_ms, sizeof(g_cfg.sds_meas_interval_ms));
		rc = sizeof(g_cfg.sds_meas_interval_ms);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_TIME_FOR_WIFI_CONFIG, NULL)) {
		if (val_len_max < (int)sizeof(g_cfg.time_for_wifi_config)) {
			rc = -ENOMEM;
			goto out;
		}
		memcpy(val, &g_cfg.time_for_wifi_config, sizeof(g_cfg.time_for_wifi_config));
		rc = sizeof(g_cfg.time_for_wifi_config);
		goto out;
	}
	if (settings_name_steq(name, CFG_KEY_STANDALONE, NULL)) {
		rc = cfg_write_bool(val, val_len_max, g_cfg.standalone);
		goto out;
	}

	rc = -ENOENT;
out:
	k_mutex_unlock(&g_cfg_lock);
	return rc;
}

static int altruist_config_settings_export(int (*cb)(const char *name,
						      const void *value,
						      size_t val_len))
{
	int rc;
	uint8_t bool_raw;

	k_mutex_lock(&g_cfg_lock, K_FOREVER);

	rc = cb(EXPORT_KEY(CFG_KEY_SCHEMA_VERSION), &g_cfg.schema_version,
		sizeof(g_cfg.schema_version));
	if (rc) {
		goto out;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_CURRENT_LANG), g_cfg.current_lang,
		strlen(g_cfg.current_lang) + 1U);
	if (rc) {
		goto out;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_WLANSSID), g_cfg.wlanssid,
		strlen(g_cfg.wlanssid) + 1U);
	if (rc) {
		goto out;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_WLANPWD), g_cfg.wlanpwd,
		strlen(g_cfg.wlanpwd) + 1U);
	if (rc) {
		goto out;
	}
	bool_raw = g_cfg.wlannopwd ? 1U : 0U;
	rc = cb(EXPORT_KEY(CFG_KEY_WLANNOPWD), &bool_raw, sizeof(bool_raw));
	if (rc) {
		goto out;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_FS_SSID), g_cfg.fs_ssid,
		strlen(g_cfg.fs_ssid) + 1U);
	if (rc) {
		goto out;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_FS_PWD), g_cfg.fs_pwd,
		strlen(g_cfg.fs_pwd) + 1U);
	if (rc) {
		goto out;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_RWS_OWNER), g_cfg.rws_owner,
		strlen(g_cfg.rws_owner) + 1U);
	if (rc) {
		goto out;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_ROBONOMICS_PUBLIC_NODE),
		g_cfg.robonomics_public_node,
		strlen(g_cfg.robonomics_public_node) + 1U);
	if (rc) {
		goto out;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_ROBONOMICS_CONNECTIVITY_HOST),
		g_cfg.robonomics_connectivity_host,
		strlen(g_cfg.robonomics_connectivity_host) + 1U);
	if (rc) {
		goto out;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_ROBONOMICS_CONNECTIVITY_HOSTS),
		g_cfg.robonomics_connectivity_hosts,
		strlen(g_cfg.robonomics_connectivity_hosts) + 1U);
	if (rc) {
		goto out;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_PRIVATE_KEY), g_cfg.private_key,
		strlen(g_cfg.private_key) + 1U);
	if (rc) {
		goto out;
	}
	bool_raw = g_cfg.send2robonomics ? 1U : 0U;
	rc = cb(EXPORT_KEY(CFG_KEY_SEND2ROBONOMICS), &bool_raw, sizeof(bool_raw));
	if (rc) {
		goto out;
	}
	bool_raw = g_cfg.send2csv ? 1U : 0U;
	rc = cb(EXPORT_KEY(CFG_KEY_SEND2CSV), &bool_raw, sizeof(bool_raw));
	if (rc) {
		goto out;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_SENDING_INTERVAL_MS), &g_cfg.sending_interval_ms,
		sizeof(g_cfg.sending_interval_ms));
	if (rc) {
		goto out;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_DATALOG_SENDING_INTERVAL_MS),
		&g_cfg.datalog_sending_interval_ms,
		sizeof(g_cfg.datalog_sending_interval_ms));
	if (rc) {
		goto out;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_SDS_MEAS_INTERVAL_MS), &g_cfg.sds_meas_interval_ms,
		sizeof(g_cfg.sds_meas_interval_ms));
	if (rc) {
		goto out;
	}
	rc = cb(EXPORT_KEY(CFG_KEY_TIME_FOR_WIFI_CONFIG), &g_cfg.time_for_wifi_config,
		sizeof(g_cfg.time_for_wifi_config));
	if (rc) {
		goto out;
	}
	bool_raw = g_cfg.standalone ? 1U : 0U;
	rc = cb(EXPORT_KEY(CFG_KEY_STANDALONE), &bool_raw, sizeof(bool_raw));
out:
	k_mutex_unlock(&g_cfg_lock);
	return rc;
}

SETTINGS_STATIC_HANDLER_DEFINE(altruist_config, CONFIG_ROOT,
			       altruist_config_settings_get,
			       altruist_config_settings_set,
			       NULL,
			       altruist_config_settings_export);

static int altruist_config_apply_migration(void)
{
	altruist_config_migrate_cb_t migrate_hook;
	uint32_t from_version;
	int rc;

	k_mutex_lock(&g_cfg_lock, K_FOREVER);
	from_version = g_cfg.schema_version;
	migrate_hook = g_migrate_hook;
	k_mutex_unlock(&g_cfg_lock);

	if (from_version >= ALTRUIST_CONFIG_SCHEMA_VERSION) {
		return 0;
	}

	if (migrate_hook != NULL) {
		k_mutex_lock(&g_cfg_lock, K_FOREVER);
		rc = migrate_hook(from_version, &g_cfg);
		k_mutex_unlock(&g_cfg_lock);
		if (rc < 0) {
			return rc;
		}
	}

	k_mutex_lock(&g_cfg_lock, K_FOREVER);
	g_cfg.schema_version = ALTRUIST_CONFIG_SCHEMA_VERSION;
	k_mutex_unlock(&g_cfg_lock);
	return altruist_config_save();
}

int altruist_config_init(void)
{
	int rc;

	k_mutex_lock(&g_cfg_lock, K_FOREVER);
	altruist_config_apply_defaults(&g_cfg);
	k_mutex_unlock(&g_cfg_lock);

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

	k_mutex_lock(&g_cfg_lock, K_FOREVER);
	g_initialized = true;
	k_mutex_unlock(&g_cfg_lock);
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
	k_mutex_lock(&g_cfg_lock, K_FOREVER);
	altruist_config_apply_defaults(&g_cfg);
	k_mutex_unlock(&g_cfg_lock);
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

	k_mutex_lock(&g_cfg_lock, K_FOREVER);
	memcpy(&g_cfg, new_cfg, sizeof(g_cfg));
	k_mutex_unlock(&g_cfg_lock);
	if (!persist) {
		return 0;
	}

	return altruist_config_save();
}

int altruist_config_set_migration_hook(altruist_config_migrate_cb_t hook)
{
	k_mutex_lock(&g_cfg_lock, K_FOREVER);
	g_migrate_hook = hook;
	k_mutex_unlock(&g_cfg_lock);
	return 0;
}

const struct altruist_config *altruist_config_get(void)
{
	k_mutex_lock(&g_cfg_lock, K_FOREVER);
	if (!g_initialized) {
		altruist_config_apply_defaults(&g_cfg);
	}
	memcpy(&g_cfg_snapshot, &g_cfg, sizeof(g_cfg_snapshot));
	k_mutex_unlock(&g_cfg_lock);

	return &g_cfg_snapshot;
}
