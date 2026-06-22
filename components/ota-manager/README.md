# OTA Manager Process Flow

The OTA manager owns the firmware download/apply step after another component
has decided that an update should run. In this project, `mqtt-manager` receives
the update URL and `main` passes that URL into `ota_manager_start()`.

## Boot Validation Flow

`ota_manager_mark_app_valid()` should run early during boot, after NVS is
initialized and before the device goes back to sleep.

1. Get the currently running OTA partition with `esp_ota_get_running_partition()`.
2. Ask ESP-IDF for that partition's OTA state with `esp_ota_get_state_partition()`.
3. If the image is `ESP_OTA_IMG_PENDING_VERIFY`, call
   `esp_ota_mark_app_valid_cancel_rollback()`.
4. If the image is not pending verification, return `ESP_OK` without changing
   OTA state.

This confirms that the newly booted firmware is healthy enough to keep. If app
rollback is enabled and the app never marks itself valid, the bootloader can
roll back to the previous OTA slot.

## Update Flow

`ota_manager_start()` runs the actual update.

1. Validate the URL pointer.
2. Validate the URL length against `CONFIG_TEMP_CUBE_OTA_URL_MAX_LEN`.
3. Accept only `https://` URLs, or `http://` URLs when
   `CONFIG_TEMP_CUBE_OTA_ALLOW_HTTP` is enabled.
4. If `CONFIG_TEMP_CUBE_OTA_SKIP_LAST_SUCCESSFUL_URL` is enabled, compare the
   URL with the last successful URL stored in NVS under:
   - namespace: `ota`
   - key: `last_url`
5. If the URL was already applied, report `OTA / CURRENT` and return
   `ESP_ERR_INVALID_STATE`.
6. Report `OTA / STARTING` through the optional status callback.
7. Build an `esp_http_client_config_t` with the URL, timeout, keep-alive, and
   optional ESP x509 certificate bundle.
8. Report `OTA / UPDATING`.
9. Call `esp_https_ota()`.
10. If the download or flash operation fails, report `OTA / FAILED` and return
    the ESP-IDF error code.
11. If OTA succeeds, store the URL in NVS as the last successful update URL.
12. Report `OTA / RESTART` and return `ESP_OK`.

The caller is responsible for restarting the ESP32-S3 after `ESP_OK` so the
bootloader can select and boot the newly written OTA partition.

## Status Callback

The OTA manager does not know about the OLED directly. Instead, callers pass an
optional `ota_manager_status_cb_t`:

```c
typedef void (*ota_manager_status_cb_t)(const char *line1, const char *line2, void *ctx);
```

`main` uses this callback to display two short lines on the SSD1306 OLED, such
as:

```text
OTA
UPDATING
```

If the callback is `NULL`, OTA still runs and only logs status through ESP-IDF.

## Return Values

- `ESP_OK`: OTA completed successfully. Restart the device.
- `ESP_ERR_INVALID_ARG`: URL is missing, empty, or too long.
- `ESP_ERR_NOT_SUPPORTED`: URL scheme is unsupported, or HTTP is disabled.
- `ESP_ERR_INVALID_STATE`: URL already matches the last successful OTA URL.
- Other ESP-IDF errors: returned from `esp_https_ota()` when download, image
  validation, or flash writing fails.

## Configuration

These options are defined in `Kconfig.projbuild`:

- `CONFIG_TEMP_CUBE_OTA_URL_MAX_LEN`: maximum accepted firmware URL length.
- `CONFIG_TEMP_CUBE_OTA_HTTP_TIMEOUT_MS`: HTTP client timeout during download.
- `CONFIG_TEMP_CUBE_OTA_ALLOW_HTTP`: allows plain HTTP OTA URLs for development.
- `CONFIG_TEMP_CUBE_OTA_USE_CERT_BUNDLE`: attaches ESP-IDF's certificate bundle
  for HTTPS downloads.
- `CONFIG_TEMP_CUBE_OTA_SKIP_LAST_SUCCESSFUL_URL`: prevents retained MQTT OTA
  messages from reflashing the same URL on every wake cycle.

## High-Level Sequence

```text
Boot
  -> wifi_manager_init()
  -> ota_manager_mark_app_valid()
  -> connect WiFi
  -> connect MQTT
  -> wait for OTA URL trigger
  -> ota_manager_start(url, display_callback, display_handle)
       -> validate URL
       -> skip if already applied
       -> show OTA STARTING
       -> show OTA UPDATING
       -> esp_https_ota()
       -> remember successful URL
       -> show OTA RESTART
  -> esp_restart()
  -> bootloader starts new OTA image
```
