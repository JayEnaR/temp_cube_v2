#pragma once

#include <stdint.h>

#define SSID_MAX_LEN    33
#define SSID_LIST_MAX   20

typedef struct {
    uint16_t ap_count;
    char ssids[SSID_LIST_MAX][SSID_MAX_LEN];
} wifi_scan_results_t;

void start_http_server(void);
void stop_http_server(void);

/* Called by soft-ap after a WiFi scan completes */
void http_server_set_scan_results(const wifi_scan_results_t *results);
