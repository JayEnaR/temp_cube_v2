#include "ota-manager.h"

#include <stdbool.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "nvs.h"

static const char *TAG = "ota-manager";

#define OTA_NVS_NAMESPACE     "ota"
#define OTA_NVS_KEY_LAST_URL  "last_url"

static void report_status(ota_manager_status_cb_t status_cb, void *ctx,
                          const char *line1, const char *line2)
{
    if (status_cb != NULL) {
        status_cb(line1, line2, ctx);
    }
}

static bool ota_url_was_applied(const char *url)
{
    char last_url[CONFIG_TEMP_CUBE_OTA_URL_MAX_LEN] = {0};
    size_t last_url_len = sizeof(last_url);
    nvs_handle_t handle;

    if (nvs_open(OTA_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    esp_err_t err = nvs_get_str(handle, OTA_NVS_KEY_LAST_URL, last_url, &last_url_len);
    nvs_close(handle);

    return err == ESP_OK && strcmp(last_url, url) == 0;
}

static void remember_applied_ota_url(const char *url)
{
    nvs_handle_t handle;

    esp_err_t err = nvs_open(OTA_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open OTA NVS namespace: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_str(handle, OTA_NVS_KEY_LAST_URL, url);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to remember OTA URL: %s", esp_err_to_name(err));
    }
}

esp_err_t ota_manager_mark_app_valid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;

    esp_err_t err = esp_ota_get_state_partition(running, &state);
    if (err != ESP_OK) {
        return ESP_OK;
    }

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "OTA image is pending verification, marking valid");
        return esp_ota_mark_app_valid_cancel_rollback();
    }

    return ESP_OK;
}

esp_err_t ota_manager_start(const char *url, ota_manager_status_cb_t status_cb, void *ctx)
{
    if (url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t url_len = strnlen(url, CONFIG_TEMP_CUBE_OTA_URL_MAX_LEN);
    if (url_len == 0 || url_len >= CONFIG_TEMP_CUBE_OTA_URL_MAX_LEN) {
        ESP_LOGW(TAG, "OTA URL is empty or too long");
        return ESP_ERR_INVALID_ARG;
    }

    bool is_https = strncmp(url, "https://", 8) == 0;
    bool is_http = strncmp(url, "http://", 7) == 0;
    if (!is_https && !is_http) {
        ESP_LOGW(TAG, "Unsupported OTA URL scheme: %s", url);
        return ESP_ERR_NOT_SUPPORTED;
    }

#if !CONFIG_TEMP_CUBE_OTA_ALLOW_HTTP
    if (is_http) {
        ESP_LOGW(TAG, "Plain HTTP OTA is disabled");
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif

#if CONFIG_TEMP_CUBE_OTA_SKIP_LAST_SUCCESSFUL_URL
    if (ota_url_was_applied(url)) {
        ESP_LOGI(TAG, "OTA URL was already applied, skipping: %s", url);
        report_status(status_cb, ctx, "OTA", "CURRENT");
        return ESP_ERR_INVALID_STATE;
    }
#endif

    report_status(status_cb, ctx, "OTA", "STARTING");
    ESP_LOGI(TAG, "Starting OTA from %s", url);

    esp_http_client_config_t http_config = {
        .url = url,
        .timeout_ms = CONFIG_TEMP_CUBE_OTA_HTTP_TIMEOUT_MS,
        .keep_alive_enable = true,
#if CONFIG_TEMP_CUBE_OTA_USE_CERT_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    report_status(status_cb, ctx, "OTA", "UPDATING");
    esp_err_t err = esp_https_ota(&ota_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(err));
        report_status(status_cb, ctx, "OTA", "FAILED");
        return err;
    }

    remember_applied_ota_url(url);
    report_status(status_cb, ctx, "OTA", "RESTART");
    ESP_LOGI(TAG, "OTA complete, restart required");
    return ESP_OK;
}
