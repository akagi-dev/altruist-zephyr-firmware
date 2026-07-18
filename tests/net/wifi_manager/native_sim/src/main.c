#include <string.h>

#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#include <altruist/provisioning.h>
#include <altruist/wifi_manager.h>

static bool test_has_credentials;
static bool test_force_provisioning;
static int test_connect_attempts;
static int test_connect_result;

bool altruist_config_get_wifi_credentials(struct wifi_manager_credentials *out)
{
	if (!test_has_credentials) {
		return false;
	}

	memset(out, 0, sizeof(*out));
	memcpy(out->ssid, "ssid", sizeof("ssid"));
	memcpy(out->psk, "psk", sizeof("psk"));
	return true;
}

bool altruist_config_is_provisioning_forced(void)
{
	return test_force_provisioning;
}

int altruist_wifi_request_connect(const struct wifi_manager_credentials *credentials)
{
	ARG_UNUSED(credentials);
	test_connect_attempts++;
	return test_connect_result;
}

static void reset_mocks(void)
{
	test_has_credentials = true;
	test_force_provisioning = false;
	test_connect_attempts = 0;
	test_connect_result = 0;
}

ZTEST(wifi_manager, test_reconnect_backoff_is_bounded)
{
	reset_mocks();
	zassert_ok(wifi_manager_init(), NULL);
	zassert_ok(wifi_manager_start(), NULL);
	zassert_equal(test_connect_attempts, 1, NULL);
	zassert_equal(wifi_manager_get_state(), WIFI_MANAGER_STATE_CONNECTING, NULL);

	wifi_manager_handle_net_event(NET_EVENT_WIFI_DISCONNECT_RESULT);
	zassert_true(wifi_manager_is_reconnect_scheduled(), NULL);
	zassert_equal(wifi_manager_get_reconnect_remaining_ms(), 100, NULL);
	zassert_equal(wifi_manager_get_next_backoff_ms(), 200, NULL);

	wifi_manager_advance_time(99);
	zassert_equal(test_connect_attempts, 1, NULL);
	wifi_manager_advance_time(1);
	zassert_equal(test_connect_attempts, 2, NULL);

	wifi_manager_handle_net_event(NET_EVENT_WIFI_DISCONNECT_RESULT);
	zassert_equal(wifi_manager_get_reconnect_remaining_ms(), 200, NULL);
	zassert_equal(wifi_manager_get_next_backoff_ms(), 400, NULL);
	wifi_manager_advance_time(200);
	zassert_equal(test_connect_attempts, 3, NULL);

	wifi_manager_handle_net_event(NET_EVENT_WIFI_DISCONNECT_RESULT);
	zassert_equal(wifi_manager_get_reconnect_remaining_ms(), 400, NULL);
	zassert_equal(wifi_manager_get_next_backoff_ms(), 400, NULL);
}

ZTEST(wifi_manager, test_provisioning_mode_without_credentials)
{
	reset_mocks();
	test_has_credentials = false;

	zassert_ok(wifi_manager_init(), NULL);
	zassert_ok(wifi_manager_start(), NULL);
	zassert_equal(test_connect_attempts, 0, NULL);
	zassert_true(altruist_provisioning_is_active(), NULL);
	zassert_equal(wifi_manager_get_state(), WIFI_MANAGER_STATE_PROVISIONING, NULL);

	test_has_credentials = true;
	zassert_ok(wifi_manager_refresh_configuration(), NULL);
	zassert_false(altruist_provisioning_is_active(), NULL);
	zassert_equal(wifi_manager_get_state(), WIFI_MANAGER_STATE_CONNECTING, NULL);
	zassert_equal(test_connect_attempts, 1, NULL);
}

ZTEST(wifi_manager, test_connected_event_resets_backoff)
{
	reset_mocks();
	zassert_ok(wifi_manager_init(), NULL);
	zassert_ok(wifi_manager_start(), NULL);
	wifi_manager_handle_net_event(NET_EVENT_WIFI_DISCONNECT_RESULT);
	wifi_manager_advance_time(100);
	zassert_equal(wifi_manager_get_next_backoff_ms(), 200, NULL);

	wifi_manager_handle_net_event(NET_EVENT_WIFI_CONNECT_RESULT);
	zassert_equal(wifi_manager_get_state(), WIFI_MANAGER_STATE_CONNECTED, NULL);
	zassert_equal(wifi_manager_get_next_backoff_ms(), 100, NULL);

	wifi_manager_handle_net_event(NET_EVENT_WIFI_DISCONNECT_RESULT);
	zassert_equal(wifi_manager_get_reconnect_remaining_ms(), 100, NULL);
}

ZTEST_SUITE(wifi_manager, NULL, NULL, NULL, NULL, NULL);
