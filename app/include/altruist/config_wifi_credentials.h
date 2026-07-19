#pragma once

#include <altruist/wifi_manager.h>

int altruist_config_set_wifi_credentials(const struct wifi_manager_credentials *credentials);
int altruist_config_clear_wifi_credentials(void);
