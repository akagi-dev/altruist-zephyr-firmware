#pragma once

#include <stdbool.h>

#include <altruist/wifi_manager.h>

bool altruist_config_get_wifi_credentials(struct wifi_manager_credentials *out);
int altruist_config_set_wifi_credentials(const struct wifi_manager_credentials *credentials);
int altruist_config_clear_wifi_credentials(void);
