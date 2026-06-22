#include "button.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define BUTTON_GPIO         ((gpio_num_t)CONFIG_BUTTON_GPIO)
#define LONG_PRESS_MS       (CONFIG_BUTTON_LONG_PRESS_SEC * 1000)
#define POLL_MS             20

static const char *TAG = "button";

static bool button_is_pressed(void)
{
    return gpio_get_level(BUTTON_GPIO) == 0;
}

void button_enable_wakeup_source(void)
{
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup(1ULL << BUTTON_GPIO, ESP_EXT1_WAKEUP_ANY_LOW));
}

static void button_enter_button_only_sleep(void)
{
    ESP_LOGI(TAG, "Short press detected, entering button-only deep sleep");
    button_enable_wakeup_source();
    esp_deep_sleep_start();
}

static void button_reset_nvs_and_restart(int64_t held_ms)
{
    ESP_LOGW(TAG, "Long press (%lldms), erasing NVS and restarting", held_ms);
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_flash_erase());
    esp_restart();
}

static void button_task(void *arg)
{
    bool pressed = button_is_pressed();
    bool initial_press = pressed;
    bool long_triggered = false;
    int64_t press_start_ms = pressed ? esp_timer_get_time() / 1000 : 0;
    int prev_level = pressed ? 0 : 1;

    ESP_LOGI(TAG, "Button armed on GPIO%d, hold %ds to erase NVS (idle=%d)",
             BUTTON_GPIO, CONFIG_BUTTON_LONG_PRESS_SEC, prev_level);

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));

        int level = gpio_get_level(BUTTON_GPIO);
        if (level != prev_level) {
            ESP_LOGI(TAG, "GPIO%d: %d -> %d", BUTTON_GPIO, prev_level, level);
            prev_level = level;
        }

        if (level == 0 && !pressed) {
            pressed = true;
            initial_press = false;
            long_triggered = false;
            press_start_ms = esp_timer_get_time() / 1000;
            ESP_LOGI(TAG, "Button pressed");
        } else if (level == 1 && pressed) {
            int64_t held = (esp_timer_get_time() / 1000) - press_start_ms;
            bool was_initial = initial_press;

            pressed = false;
            initial_press = false;
            ESP_LOGI(TAG, "Button released, held %lldms", held);

            if (!long_triggered && !was_initial) {
                button_enter_button_only_sleep();
            }
        } else if (level == 0 && pressed && !long_triggered) {
            int64_t held = (esp_timer_get_time() / 1000) - press_start_ms;
            if (held >= LONG_PRESS_MS) {
                long_triggered = true;
                button_reset_nvs_and_restart(held);
            }
        }
    }
}

void button_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    BaseType_t ok = xTaskCreate(button_task, "button", 2048, NULL, 5, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button task");
    }
}
