#include <stdio.h>
#include <assert.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <ssd1306.h>

#define APP_TAG         "app_main"

#define I2C_PORT        I2C_NUM_0
#define I2C_SDA_GPIO    GPIO_NUM_41
#define I2C_SCL_GPIO    GPIO_NUM_40

void app_main(void)
{
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

    // 64x32 display: 8 chars max per line (8px/char), 4 pages (rows of 8px)
    ssd1306_display_text(dev_hdl, 0, "Hello", false);
}
