#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_event.h"
#include "temp_cube_bme280.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(MQTT_MANAGER_EVENT);

typedef enum {
    MQTT_MANAGER_EVENT_CONNECTED,
    MQTT_MANAGER_EVENT_DISCONNECTED,
} mqtt_manager_event_t;

void mqtt_manager_init(void);
void mqtt_manager_deinit(void);

/**
 * Publish all values in a BME280 reading as JSON to CONFIG_MQTT_READINGS_TOPIC.
 */
esp_err_t mqtt_manager_publish_reading(const temp_cube_bme280_reading_t *reading,
                                       uint32_t connect_timeout_ms);

#ifdef __cplusplus
}
#endif
