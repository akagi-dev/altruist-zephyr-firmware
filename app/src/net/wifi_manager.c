/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Baseline logic adapted for Zephyr from airalab/altruist-firmware (wifi_manager.cpp). */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/util.h>

#include <altruist/provisioning.h>
#include <altruist/wifi_manager.h>

/*
 * WP-04 config subsystem seam:
 * - Provide strong definitions from the config package later.
 * - Keep this baseline independent from app/src/config implementation details.
 */
__weak bool altruist_config_get_wifi_credentials(struct wifi_manager_credentials *out)
{
	ARG_UNUSED(out);
	return false;
}

__weak bool altruist_config_is_provisioning_forced(void)
{
	return false;
}

__weak int altruist_wifi_request_connect(const struct wifi_manager_credentials *credentials)
{
	ARG_UNUSED(credentials);

#if defined(CONFIG_NET_MGMT_EVENT) && defined(CONFIG_NETWORKING) && defined(CONFIG_WIFI)
	struct net_if *iface = net_if_get_default();
	struct wifi_connect_req_params params = { 0 };
	size_t ssid_len;
	size_t psk_len;

	if (iface == NULL || credentials == NULL) {
		return -EINVAL;
	}

	ssid_len = strnlen(credentials->ssid, sizeof(credentials->ssid));
	if (ssid_len == 0) {
		return -EINVAL;
	}

	psk_len = strnlen(credentials->psk, sizeof(credentials->psk));
	params.ssid = credentials->ssid;
	params.ssid_length = ssid_len;
	params.channel = WIFI_CHANNEL_ANY;

	if (psk_len > 0U) {
		params.psk = credentials->psk;
		params.psk_length = psk_len;
		params.security = WIFI_SECURITY_TYPE_PSK;
	} else {
		params.security = WIFI_SECURITY_TYPE_NONE;
	}

	return net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
#else
	return -ENOTSUP;
#endif
}

static struct wifi_manager_credentials cached_credentials;
static enum wifi_manager_state wifi_state;
static uint32_t reconnect_remaining_ms;
static uint32_t next_backoff_ms;
static bool reconnect_scheduled;
static struct k_work_delayable reconnect_work;
static bool reconnect_work_initialized;
K_MUTEX_DEFINE(wifi_manager_lock);

#define WIFI_CLAMPED_INITIAL_BACKOFF_MS                                                 \
	MIN(CONFIG_ALTRUIST_WIFI_RECONNECT_INITIAL_BACKOFF_MS,                          \
	    CONFIG_ALTRUIST_WIFI_RECONNECT_MAX_BACKOFF_MS)

static void wifi_lock(void)
{
	k_mutex_lock(&wifi_manager_lock, K_FOREVER);
}

static void wifi_unlock(void)
{
	k_mutex_unlock(&wifi_manager_lock);
}

static void wifi_clear_cached_credentials(void)
{
	memset(&cached_credentials, 0, sizeof(cached_credentials));
}

static int wifi_connect_now(void);
static void wifi_on_disconnected(void);
static void wifi_handle_net_event_locked(uint32_t mgmt_event);

static void wifi_reconnect_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	wifi_lock();
	reconnect_scheduled = false;
	reconnect_remaining_ms = 0U;
	(void)wifi_connect_now();
	wifi_unlock();
}

#if defined(CONFIG_NET_MGMT_EVENT)
static struct net_mgmt_event_callback net_cb;
static bool net_cb_registered;

static void wifi_net_event_callback(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface)
{
	const struct wifi_status *status;

	ARG_UNUSED(iface);

	wifi_lock();
	if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT && cb->info != NULL &&
	    cb->info_length >= sizeof(*status)) {
		status = cb->info;
		if (status->status != 0) {
			wifi_on_disconnected();
			wifi_unlock();
			return;
		}
	}

	wifi_handle_net_event_locked(mgmt_event);
	wifi_unlock();
}
#endif

static void wifi_reset_backoff(void)
{
	next_backoff_ms = WIFI_CLAMPED_INITIAL_BACKOFF_MS;
}

static void wifi_cancel_reconnect(void)
{
	reconnect_scheduled = false;
	reconnect_remaining_ms = 0U;

	if (reconnect_work_initialized) {
		(void)k_work_cancel_delayable(&reconnect_work);
	}
}

static void wifi_schedule_reconnect(void)
{
	uint32_t max_backoff = CONFIG_ALTRUIST_WIFI_RECONNECT_MAX_BACKOFF_MS;

	reconnect_remaining_ms = next_backoff_ms;
	reconnect_scheduled = true;
	if (reconnect_work_initialized) {
		(void)k_work_reschedule(&reconnect_work, K_MSEC(reconnect_remaining_ms));
	}

	if (next_backoff_ms >= max_backoff) {
		next_backoff_ms = max_backoff;
		return;
	}

	/* If next > max/2, doubling would overflow or cross max; clamp directly. */
	if (next_backoff_ms > (max_backoff / 2U)) {
		next_backoff_ms = max_backoff;
		return;
	}

	next_backoff_ms *= 2U;
}

static int wifi_connect_now(void)
{
	int rc = altruist_wifi_request_connect(&cached_credentials);

	if (rc == 0) {
		wifi_state = WIFI_MANAGER_STATE_CONNECTING;
		return 0;
	}

	wifi_state = WIFI_MANAGER_STATE_DISCONNECTED;
	wifi_schedule_reconnect();
	return rc;
}

static void wifi_on_connected(void)
{
	wifi_cancel_reconnect();
	wifi_reset_backoff();
	wifi_state = WIFI_MANAGER_STATE_CONNECTED;
	(void)altruist_provisioning_stop();
}

static void wifi_on_disconnected(void)
{
	if (wifi_state == WIFI_MANAGER_STATE_PROVISIONING) {
		return;
	}

	wifi_state = WIFI_MANAGER_STATE_DISCONNECTED;
	wifi_schedule_reconnect();
}

int wifi_manager_init(void)
{
	wifi_lock();

	wifi_state = WIFI_MANAGER_STATE_DISCONNECTED;
	wifi_cancel_reconnect();
	wifi_clear_cached_credentials();
	wifi_reset_backoff();

	if (!reconnect_work_initialized) {
		k_work_init_delayable(&reconnect_work, wifi_reconnect_work_handler);
		reconnect_work_initialized = true;
	}

#if defined(CONFIG_NET_MGMT_EVENT)
	if (!net_cb_registered) {
		net_mgmt_init_event_callback(&net_cb, wifi_net_event_callback,
					     NET_EVENT_WIFI_CONNECT_RESULT |
						     NET_EVENT_WIFI_DISCONNECT_RESULT);
		net_mgmt_add_event_callback(&net_cb);
		net_cb_registered = true;
	}
#endif

	wifi_unlock();
	return 0;
}

int wifi_manager_refresh_configuration(void)
{
	struct wifi_manager_credentials new_credentials = { 0 };
	bool force_provisioning = altruist_config_is_provisioning_forced();
	bool has_credentials = altruist_config_get_wifi_credentials(&new_credentials);
	int rc;

	wifi_lock();
	wifi_cancel_reconnect();

	if (force_provisioning || !has_credentials) {
		wifi_state = WIFI_MANAGER_STATE_PROVISIONING;
		wifi_clear_cached_credentials();
		rc = altruist_provisioning_start_ap_mode();
		wifi_unlock();
		return rc;
	}

	cached_credentials = new_credentials;
	(void)altruist_provisioning_stop();
	rc = wifi_connect_now();
	wifi_unlock();
	return rc;
}

int wifi_manager_start(void)
{
	return wifi_manager_refresh_configuration();
}

void wifi_manager_handle_net_event(uint32_t mgmt_event)
{
	wifi_lock();
	wifi_handle_net_event_locked(mgmt_event);
	wifi_unlock();
}

static void wifi_handle_net_event_locked(uint32_t mgmt_event)
{
	if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		wifi_on_connected();
		return;
	}

	if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
		wifi_on_disconnected();
		return;
	}
}

void wifi_manager_advance_time(uint32_t elapsed_ms)
{
	wifi_lock();
	if (!reconnect_scheduled) {
		wifi_unlock();
		return;
	}

	if (elapsed_ms < reconnect_remaining_ms) {
		reconnect_remaining_ms -= elapsed_ms;
		wifi_unlock();
		return;
	}

	reconnect_scheduled = false;
	reconnect_remaining_ms = 0U;
	if (reconnect_work_initialized) {
		(void)k_work_cancel_delayable(&reconnect_work);
	}
	(void)wifi_connect_now();
	wifi_unlock();
}

enum wifi_manager_state wifi_manager_get_state(void)
{
	enum wifi_manager_state state;

	wifi_lock();
	state = wifi_state;
	wifi_unlock();
	return state;
}

bool wifi_manager_is_reconnect_scheduled(void)
{
	bool scheduled;

	wifi_lock();
	scheduled = reconnect_scheduled;
	wifi_unlock();
	return scheduled;
}

uint32_t wifi_manager_get_reconnect_remaining_ms(void)
{
	uint32_t remaining;

	wifi_lock();
	remaining = reconnect_remaining_ms;
	wifi_unlock();
	return remaining;
}

uint32_t wifi_manager_get_next_backoff_ms(void)
{
	uint32_t backoff;

	wifi_lock();
	backoff = next_backoff_ms;
	wifi_unlock();
	return backoff;
}
