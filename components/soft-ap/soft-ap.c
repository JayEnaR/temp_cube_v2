#include <string.h>
#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "soft-ap.h"
#include "http-server.h"

#define AP_SSID     CONFIG_SOFT_AP_SSID
#define AP_PASS     CONFIG_SOFT_AP_PASSWORD
#define AP_CHANNEL  CONFIG_SOFT_AP_CHANNEL
#define AP_MAX_CONN CONFIG_SOFT_AP_MAX_STA_CONN

static const char *TAG = "soft-ap";

static esp_netif_t *s_ap_netif = NULL;
static esp_event_handler_instance_t s_scan_handle = NULL;

/* Called by http-server when scan results are available */
void on_wifi_scan_res(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    /* Forwarded to http-server via http_server_set_scan_results() */
    http_server_set_scan_results((wifi_scan_results_t *)event_data);
}

static void on_wifi_scan_done(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    wifi_scan_results_t *results = malloc(sizeof(wifi_scan_results_t));
    if (!results) {
        return;
    }
    memset(results, 0, sizeof(wifi_scan_results_t));

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    ESP_LOGI(TAG, "Scan found %d APs", ap_count);

    if (ap_count > 0) {
        if (ap_count > SSID_LIST_MAX) {
            ap_count = SSID_LIST_MAX;
        }
        wifi_ap_record_t *ap_info = malloc(sizeof(wifi_ap_record_t) * ap_count);
        if (ap_info) {
            esp_wifi_scan_get_ap_records(&ap_count, ap_info);
            results->ap_count = ap_count;
            for (int i = 0; i < ap_count; i++) {
                strncpy(results->ssids[i], (char *)ap_info[i].ssid, SSID_MAX_LEN - 1);
                results->ssids[i][SSID_MAX_LEN - 1] = '\0';
            }
            free(ap_info);
        }
        http_server_set_scan_results(results);
    }

    free(results);
    esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, s_scan_handle);
    s_scan_handle = NULL;
}

static void wifi_scan_async(void)
{
    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
    };
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE,
                                        &on_wifi_scan_done, NULL, &s_scan_handle);
    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(ret));
    }
}

static void wifi_init_softap(void)
{
    ESP_LOGI(TAG, "Initializing soft-AP (SSID: %s)", AP_SSID);

    s_ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = AP_CHANNEL,
            .password = AP_PASS,
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = { .required = false },
        },
    };

    if (strlen(AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_scan_async();
}

void start_soft_ap(void)
{
    ESP_LOGI(TAG, "Starting soft-AP");

    if (s_ap_netif != NULL) {
        esp_wifi_stop();
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }

    wifi_init_softap();
    start_http_server();
}

void stop_soft_ap(void)
{
    stop_http_server();
}
