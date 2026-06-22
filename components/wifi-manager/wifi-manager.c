#include "wifi-manager.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <inttypes.h>
#include <string.h>

#include "soft-ap.h"

static const char *TAG = "wifi-manager";

#define NVS_NAMESPACE   "storage"
#define NVS_KEY_MODE    "wifi_mode"
#define NVS_KEY_SSID    "wifi_ssid"
#define NVS_KEY_PASS    "wifi_pass"
#define WIFI_CONNECTED_BIT BIT0

ESP_EVENT_DEFINE_BASE(WIFI_MANAGER_EVENT);

static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_netif_t *s_sta_netif = NULL;
static esp_event_handler_instance_t s_sta_start_handle = NULL;
static esp_event_handler_instance_t s_sta_disconnected_handle = NULL;
static esp_event_handler_instance_t s_got_ip_handle = NULL;
static bool s_wifi_initialized = false;
static bool s_sta_should_reconnect = false;

/* ---------- NVS helpers ---------- */

void set_wifi_mode(wifi_mode_t mode)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_i32(h, NVS_KEY_MODE, (int32_t)mode));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);
    ESP_LOGI(TAG, "WiFi mode stored: %d", mode);
}

wifi_mode_t get_wifi_mode(void)
{
    nvs_handle_t h;
    int32_t mode = WIFI_MODE_NULL;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err == ESP_OK) {
        nvs_get_i32(h, NVS_KEY_MODE, &mode);
        nvs_close(h);
    }
    return (wifi_mode_t)mode;
}

bool wifi_manager_get_credentials(char *ssid, size_t ssid_len,
                                  char *pass, size_t pass_len)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return false;
    }

    err = nvs_get_str(h, NVS_KEY_SSID, ssid, &ssid_len);
    if (err != ESP_OK) {
        nvs_close(h);
        return false;
    }

    err = nvs_get_str(h, NVS_KEY_PASS, pass, &pass_len);
    nvs_close(h);
    return (err == ESP_OK);
}

void wifi_manager_save_credentials(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_str(h, NVS_KEY_SSID, ssid));
    ESP_ERROR_CHECK(nvs_set_str(h, NVS_KEY_PASS, pass));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);
    ESP_LOGI(TAG, "Credentials saved (SSID: %s)", ssid);
}

void wifi_manager_erase_credentials(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        nvs_erase_key(h, NVS_KEY_SSID);
        nvs_erase_key(h, NVS_KEY_PASS);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "Credentials erased");
}

/* ---------- STA event handlers ---------- */

static void on_sta_start(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    ESP_LOGI(TAG, "STA started, connecting...");
    esp_wifi_connect();
}

static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    set_wifi_mode(WIFI_MODE_STA);
    if (s_wifi_event_group != NULL) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    esp_event_post(WIFI_MANAGER_EVENT, WIFI_MANAGER_EVENT_CONNECTED, NULL, 0, portMAX_DELAY);
}

static void on_disconnected(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)data;
    ESP_LOGW(TAG, "Disconnected (reason: %d)%s", event->reason,
             s_sta_should_reconnect ? ", retrying..." : "");
    if (s_wifi_event_group != NULL) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    if (s_sta_should_reconnect) {
        esp_wifi_connect();
    }
}

/* ---------- Public init ---------- */

void wifi_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}

void wifi_manager_start_provisioning(void)
{
    ESP_LOGI(TAG, "Starting WiFi provisioning");
    start_soft_ap();
}

esp_err_t wifi_manager_connect_sta(uint32_t timeout_ms)
{
    char ssid[33] = {0};
    char pass[65] = {0};

    if (!wifi_manager_get_credentials(ssid, sizeof(ssid), pass, sizeof(pass))) {
        ESP_LOGW(TAG, "No WiFi credentials found");
        return ESP_ERR_NOT_FOUND;
    }

    if (s_sta_netif != NULL || s_wifi_initialized) {
        wifi_manager_disconnect();
    }

    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
        if (s_wifi_event_group == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

    ESP_LOGI(TAG, "Starting STA (SSID: %s)", ssid);

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        wifi_manager_disconnect();
        return err;
    }
    s_wifi_initialized = true;
    s_sta_should_reconnect = true;

    err = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START,
                                              &on_sta_start, NULL, &s_sta_start_handle);
    if (err != ESP_OK) {
        wifi_manager_disconnect();
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                              &on_disconnected, NULL, &s_sta_disconnected_handle);
    if (err != ESP_OK) {
        wifi_manager_disconnect();
        return err;
    }

    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &on_got_ip, NULL, &s_got_ip_handle);
    if (err != ESP_OK) {
        wifi_manager_disconnect();
        return err;
    }

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        wifi_manager_disconnect();
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        wifi_manager_disconnect();
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        wifi_manager_disconnect();
        return err;
    }

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    if ((bits & WIFI_CONNECTED_BIT) == 0) {
        ESP_LOGW(TAG, "STA connect timed out after %" PRIu32 " ms", timeout_ms);
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

void wifi_manager_disconnect(void)
{
    s_sta_should_reconnect = false;

    if (s_sta_start_handle != NULL) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, s_sta_start_handle);
        s_sta_start_handle = NULL;
    }
    if (s_sta_disconnected_handle != NULL) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, s_sta_disconnected_handle);
        s_sta_disconnected_handle = NULL;
    }
    if (s_got_ip_handle != NULL) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_got_ip_handle);
        s_got_ip_handle = NULL;
    }

    if (s_wifi_initialized) {
        esp_wifi_disconnect();
        esp_wifi_stop();
        esp_wifi_deinit();
        s_wifi_initialized = false;
    }

    if (s_sta_netif != NULL) {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
    }

    if (s_wifi_event_group != NULL) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
