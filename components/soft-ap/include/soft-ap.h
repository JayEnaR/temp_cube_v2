#pragma once

#include "esp_event.h"
#include "http-server.h"

void start_soft_ap(void);
void stop_soft_ap(void);
void on_wifi_scan_res(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data);
