#include <stdio.h>
#include <assert.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c_master.h>
#include <ssd1306.h>
#include <temp_cube_bme280.h>
#include <wifi-manager.h>

#define APP_TAG         "app_main"

#define I2C_PORT        I2C_NUM_0
#define I2C_SDA_GPIO    GPIO_NUM_41
#define I2C_SCL_GPIO    GPIO_NUM_40

static void display_padded_line(ssd1306_handle_t display, uint8_t page, const char *text)
{
    char line[9];
    snprintf(line, sizeof(line), "%-8.8s", text);
    ESP_ERROR_CHECK(ssd1306_display_text(display, page, line, false));
}

void app_main(void)
{
    wifi_manager_init();

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

    temp_cube_bme280_handle_t bme280;
    ESP_ERROR_CHECK(temp_cube_bme280_init(i2c0_bus_hdl, &bme280));

    // 64x32 display: 8 chars max per line (8px/char), 4 pages (rows of 8px)
    while (true) {
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
        } else {
            ESP_LOGE(APP_TAG, "BME280 read failed: %s", esp_err_to_name(ret));
            display_padded_line(dev_hdl, 0, "BME280");
            display_padded_line(dev_hdl, 1, "read");
            display_padded_line(dev_hdl, 2, "failed");
            display_padded_line(dev_hdl, 3, esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(temp_cube_bme280_update_interval_ms()));
    }
}
