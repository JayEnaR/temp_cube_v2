#include <stdio.h>
#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c_master.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <button.h>
#include <mqtt-manager.h>
#include <ota-manager.h>
#include <ssd1306.h>
#include <temp_cube_bme280.h>
#include <wifi-manager.h>

#define APP_TAG         "app_main"

#define I2C_PORT        I2C_NUM_0
#define I2C_SDA_GPIO    GPIO_NUM_41
#define I2C_SCL_GPIO    GPIO_NUM_40
#define WIFI_CONNECT_TIMEOUT_MS    15000
#define DISPLAY_ON_TIME_MS         6000
#define DEEP_SLEEP_TIME_US         (60ULL * 1000ULL * 1000ULL)

static void display_padded_line(ssd1306_handle_t display, uint8_t page, const char *text)
{
    char line[9];
    snprintf(line, sizeof(line), "%-8.8s", text);
    ESP_ERROR_CHECK(ssd1306_display_text(display, page, line, false));
}

static void display_ota_status(const char *line1, const char *line2, void *ctx)
{
    ssd1306_handle_t display = (ssd1306_handle_t)ctx;

    if (display == NULL) {
        return;
    }

    ssd1306_clear_display(display, false);
    display_padded_line(display, 0, line1);
    display_padded_line(display, 1, line2);
    display_padded_line(display, 2, "");
    display_padded_line(display, 3, "");
}

void app_main(void)
{
    button_init();

    wifi_manager_init();
    ESP_ERROR_CHECK(ota_manager_mark_app_valid());

    char ssid[33] = {0};
    char pass[65] = {0};
    if (!wifi_manager_get_credentials(ssid, sizeof(ssid), pass, sizeof(pass))) {
        ESP_LOGI(APP_TAG, "No WiFi credentials provisioned, starting provisioning portal");
        wifi_manager_start_provisioning();
        vTaskDelay(portMAX_DELAY);
        return;
    }

    esp_err_t wifi_ret = wifi_manager_connect_sta(WIFI_CONNECT_TIMEOUT_MS);
    bool mqtt_started = false;
    if (wifi_ret != ESP_OK) {
        ESP_LOGW(APP_TAG, "WiFi connect failed before measurement: %s", esp_err_to_name(wifi_ret));
    } else {
        mqtt_manager_init();
        mqtt_started = true;
    }

    // initialize i2c master bus
    i2c_master_bus_config_t i2c_bus_cfg = {
        .clk_source             = I2C_CLK_SRC_DEFAULT,
        .i2c_port               = I2C_PORT,
        .sda_io_num             = I2C_SDA_GPIO,
        .scl_io_num             = I2C_SCL_GPIO,
        .glitch_ignore_cnt      = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c0_bus_hdl;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c0_bus_hdl));

    // initialize ssd1306 device configuration
    ssd1306_config_t dev_cfg = SSD1306_64x32_CONFIG_DEFAULT;
    ssd1306_handle_t dev_hdl;

    // init device
    ESP_ERROR_CHECK(ssd1306_init(i2c0_bus_hdl, &dev_cfg, &dev_hdl));
    if (dev_hdl == NULL) {
        ESP_LOGE(APP_TAG, "ssd1306 handle init failed");
        assert(dev_hdl);
    }

    ssd1306_set_contrast(dev_hdl, 0xff);
    ssd1306_clear_display(dev_hdl, false);

    if (mqtt_started) {
        char ota_url[CONFIG_TEMP_CUBE_OTA_URL_MAX_LEN] = {0};
        esp_err_t ota_trigger_ret = mqtt_manager_wait_for_ota_update(
            ota_url, sizeof(ota_url), CONFIG_MQTT_OTA_TRIGGER_WAIT_MS);
        if (ota_trigger_ret == ESP_OK) {
            esp_err_t ota_ret = ota_manager_start(ota_url, display_ota_status, dev_hdl);
            if (ota_ret == ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                mqtt_manager_deinit();
                mqtt_started = false;
                wifi_manager_disconnect();
                esp_restart();
            }

            if (ota_ret != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(APP_TAG, "OTA update failed: %s", esp_err_to_name(ota_ret));
                vTaskDelay(pdMS_TO_TICKS(2000));
                ssd1306_clear_display(dev_hdl, false);
            }
        } else if (ota_trigger_ret != ESP_ERR_TIMEOUT) {
            ESP_LOGW(APP_TAG, "OTA trigger wait failed: %s", esp_err_to_name(ota_trigger_ret));
        }
    }

    temp_cube_bme280_handle_t bme280;
    ESP_ERROR_CHECK(temp_cube_bme280_init(i2c0_bus_hdl, &bme280));

    // 64x32 display: 8 chars max per line (8px/char), 4 pages (rows of 8px)
    temp_cube_bme280_reading_t reading;

    esp_err_t ret = temp_cube_bme280_read(bme280, &reading);
    if (ret == ESP_OK) {
        char text[16];

        snprintf(text, sizeof(text), "%4.1f\xB0""C", reading.temperature_c);
        display_padded_line(dev_hdl, 0, text);
        snprintf(text, sizeof(text), "%4.1f%%", reading.humidity_percent);
        display_padded_line(dev_hdl, 1, text);
        snprintf(text, sizeof(text), "%4.0fhPa", reading.pressure_hpa);
        display_padded_line(dev_hdl, 2, text);
        snprintf(text, sizeof(text), "%4.0f M", reading.altitude_m);
        display_padded_line(dev_hdl, 3, text);

        ESP_LOGI(APP_TAG, "T=%.2f C RH=%.2f %% P=%.2f hPa Alt=%.2f m",
                 reading.temperature_c, reading.humidity_percent, reading.pressure_hpa, reading.altitude_m);
        if (mqtt_started) {
            esp_err_t mqtt_ret = mqtt_manager_publish_reading(&reading, CONFIG_MQTT_CONNECT_TIMEOUT_MS);
            if (mqtt_ret != ESP_OK) {
                ESP_LOGW(APP_TAG, "MQTT publish failed: %s", esp_err_to_name(mqtt_ret));
            }
        }
    } else {
        ESP_LOGE(APP_TAG, "BME280 read failed: %s", esp_err_to_name(ret));
        display_padded_line(dev_hdl, 0, "BME280");
        display_padded_line(dev_hdl, 1, "read");
        display_padded_line(dev_hdl, 2, "failed");
        display_padded_line(dev_hdl, 3, esp_err_to_name(ret));
    }

    vTaskDelay(pdMS_TO_TICKS(DISPLAY_ON_TIME_MS));
    ssd1306_clear_display(dev_hdl, false);
    if (mqtt_started) {
        mqtt_manager_deinit();
    }
    wifi_manager_disconnect();

    ESP_LOGI(APP_TAG, "Entering deep sleep for 60 seconds");
    button_enable_wakeup_source();
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME_US));
    esp_deep_sleep_start();
}
