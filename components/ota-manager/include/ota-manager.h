#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ota_manager_status_cb_t)(const char *line1, const char *line2, void *ctx);

/**
 * Mark a pending OTA image as valid after the app has booted far enough to run.
 */
esp_err_t ota_manager_mark_app_valid(void);

/**
 * Download and apply a firmware image from the provided URL.
 *
 * On ESP_OK, the caller should restart the device so the bootloader can run the
 * newly selected OTA image.
 */
esp_err_t ota_manager_start(const char *url, ota_manager_status_cb_t status_cb, void *ctx);

#ifdef __cplusplus
}
#endif
