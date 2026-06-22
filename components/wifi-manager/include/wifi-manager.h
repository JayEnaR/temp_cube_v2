#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"

ESP_EVENT_DECLARE_BASE(WIFI_MANAGER_EVENT);

typedef enum {
    WIFI_MANAGER_EVENT_CONNECTED,
} wifi_manager_event_t;

void wifi_manager_init(void);
void wifi_manager_start_provisioning(void);
esp_err_t wifi_manager_connect_sta(uint32_t timeout_ms);
void wifi_manager_disconnect(void);

wifi_mode_t get_wifi_mode(void);
void set_wifi_mode(wifi_mode_t mode);

bool wifi_manager_get_credentials(char *ssid, size_t ssid_len,
                                  char *pass, size_t pass_len);
void wifi_manager_save_credentials(const char *ssid, const char *pass);
void wifi_manager_erase_credentials(void);
