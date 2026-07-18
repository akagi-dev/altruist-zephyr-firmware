#pragma once

#include <stdbool.h>
#include <stdint.h>

struct wifi_manager_credentials {
	char ssid[33];
	char psk[65];
};

enum wifi_manager_state {
	WIFI_MANAGER_STATE_DISCONNECTED = 0,
	WIFI_MANAGER_STATE_CONNECTING,
	WIFI_MANAGER_STATE_CONNECTED,
	WIFI_MANAGER_STATE_PROVISIONING,
};

int wifi_manager_init(void);
int wifi_manager_start(void);
int wifi_manager_refresh_configuration(void);
void wifi_manager_handle_net_event(uint32_t mgmt_event);
void wifi_manager_advance_time(uint32_t elapsed_ms);

enum wifi_manager_state wifi_manager_get_state(void);
bool wifi_manager_is_reconnect_scheduled(void);
uint32_t wifi_manager_get_reconnect_remaining_ms(void);
uint32_t wifi_manager_get_next_backoff_ms(void);
