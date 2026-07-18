/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <string.h>

#include <altruist/config.h>

#include <zephyr/settings/settings_runtime.h>
#include <zephyr/ztest.h>

ZTEST(config_settings, test_defaults_are_applied)
{
	const struct altruist_config *cfg;
	int rc = altruist_config_init();

	zassert_equal(rc, 0, "config init failed (%d)", rc);

	cfg = altruist_config_get();
	zassert_equal(cfg->schema_version, ALTRUIST_CONFIG_SCHEMA_VERSION, "schema version default");
	zassert_equal(strcmp(cfg->current_lang, "en"), 0, "lang default");
	zassert_equal(strcmp(cfg->wlanssid, "Not Set"), 0, "ssid default");
	zassert_false(cfg->send2robonomics, "send2robonomics default");
}

ZTEST(config_settings, test_runtime_decode_and_encode)
{
	const struct altruist_config *cfg;
	char value[64];
	uint8_t flag = 1U;
	uint32_t interval = 30000U;
	int rc;

	(void)altruist_config_init();

	rc = settings_runtime_set("altruist/wlanssid", "LabNet", strlen("LabNet") + 1U);
	zassert_equal(rc, 0, "runtime set ssid failed (%d)", rc);

	rc = settings_runtime_set("altruist/send2robonomics", &flag, sizeof(flag));
	zassert_equal(rc, 0, "runtime set bool failed (%d)", rc);

	rc = settings_runtime_set("altruist/sending_interval_ms", &interval, sizeof(interval));
	zassert_equal(rc, 0, "runtime set uint failed (%d)", rc);

	cfg = altruist_config_get();
	zassert_equal(strcmp(cfg->wlanssid, "LabNet"), 0, "decoded ssid");
	zassert_true(cfg->send2robonomics, "decoded bool");
	zassert_equal(cfg->sending_interval_ms, interval, "decoded interval");

	rc = settings_runtime_get("altruist/wlanssid", value, sizeof(value));
	zassert_true(rc > 0, "runtime get ssid failed (%d)", rc);
	zassert_equal(strcmp(value, "LabNet"), 0, "encoded ssid");
}

ZTEST(config_settings, test_reset_to_defaults)
{
	const struct altruist_config *cfg;
	uint8_t flag = 1U;
	int rc;

	(void)altruist_config_init();
	rc = settings_runtime_set("altruist/send2csv", &flag, sizeof(flag));
	zassert_equal(rc, 0, "runtime set bool failed (%d)", rc);

	cfg = altruist_config_get();
	zassert_true(cfg->send2csv, "precondition send2csv true");

	rc = altruist_config_reset_defaults(false);
	zassert_equal(rc, 0, "reset defaults failed (%d)", rc);

	cfg = altruist_config_get();
	zassert_false(cfg->send2csv, "send2csv should reset");
	zassert_equal(strcmp(cfg->wlanssid, "Not Set"), 0, "ssid reset");
}

ZTEST_SUITE(config_settings, NULL, NULL, NULL, NULL, NULL);
