#include "mqtt-manager.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt-manager";

#define MQTT_CONNECTED_BIT BIT0
#define MQTT_OTA_UPDATE_BIT BIT1

ESP_EVENT_DEFINE_BASE(MQTT_MANAGER_EVENT);

static esp_mqtt_client_handle_t s_client = NULL;
static EventGroupHandle_t s_mqtt_event_group = NULL;
static bool s_connected = false;
static char s_ota_url[CONFIG_TEMP_CUBE_OTA_URL_MAX_LEN];

static bool mqtt_topic_matches(const esp_mqtt_event_handle_t event, const char *topic)
{
    size_t topic_len = strlen(topic);
    return event->topic_len == (int)topic_len && strncmp(event->topic, topic, topic_len) == 0;
}

static void mqtt_handle_ota_data(const esp_mqtt_event_handle_t event)
{
    if (!mqtt_topic_matches(event, CONFIG_MQTT_OTA_TOPIC)) {
        return;
    }

    if (event->current_data_offset != 0 || event->data_len != event->total_data_len) {
        ESP_LOGW(TAG, "Ignoring fragmented OTA trigger payload");
        return;
    }

    if (event->total_data_len <= 0 || event->total_data_len >= (int)sizeof(s_ota_url)) {
        ESP_LOGW(TAG, "Ignoring invalid OTA trigger URL length: %d", event->total_data_len);
        return;
    }

    memcpy(s_ota_url, event->data, event->total_data_len);
    s_ota_url[event->total_data_len] = '\0';
    ESP_LOGI(TAG, "OTA trigger received: %s", s_ota_url);

    if (s_mqtt_event_group != NULL) {
        xEventGroupSetBits(s_mqtt_event_group, MQTT_OTA_UPDATE_BIT);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id) {
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "Connecting to broker...");
        break;

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to broker");
        s_connected = true;
        xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
        esp_mqtt_client_subscribe(event->client, CONFIG_MQTT_OTA_TOPIC, CONFIG_MQTT_QOS);
        ESP_LOGI(TAG, "Subscribed to OTA topic: %s", CONFIG_MQTT_OTA_TOPIC);
        esp_event_post(MQTT_MANAGER_EVENT, MQTT_MANAGER_EVENT_CONNECTED, NULL, 0, portMAX_DELAY);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from broker");
        s_connected = false;
        xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
        esp_event_post(MQTT_MANAGER_EVENT, MQTT_MANAGER_EVENT_DISCONNECTED, NULL, 0, portMAX_DELAY);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "Publish acknowledged (msg_id=%d)", event->msg_id);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error occurred");
        break;

    case MQTT_EVENT_DATA:
        mqtt_handle_ota_data(event);
        break;

    default:
        break;
    }
}

void mqtt_manager_init(void)
{
    if (s_client != NULL) {
        ESP_LOGW(TAG, "MQTT client already initialized");
        return;
    }

    if (s_mqtt_event_group == NULL) {
        s_mqtt_event_group = xEventGroupCreate();
        if (s_mqtt_event_group == NULL) {
            ESP_LOGE(TAG, "Failed to create MQTT event group");
            return;
        }
    }
    xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
    xEventGroupClearBits(s_mqtt_event_group, MQTT_OTA_UPDATE_BIT);

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = CONFIG_MQTT_BROKER_URI,
            },
        },
        .session = {
            .keepalive = CONFIG_MQTT_KEEPALIVE_SEC,
            .disable_keepalive = false,
            .disable_clean_session = false,
        },
        .network = {
            .reconnect_timeout_ms = 5000,
        },
        .task = {
            .priority = 5,
            .stack_size = 4096,
        },
        .buffer = {
            .size = 1024,
            .out_size = 1024,
        },
        .credentials = {
            .set_null_client_id = true,
        },
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_client));
    ESP_LOGI(TAG, "MQTT client started, broker: %s", CONFIG_MQTT_BROKER_URI);
}

void mqtt_manager_deinit(void)
{
    if (s_client == NULL) {
        return;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_stop(s_client));
    esp_mqtt_client_destroy(s_client);
    s_client = NULL;
    s_connected = false;

    if (s_mqtt_event_group != NULL) {
        xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
    }

    ESP_LOGI(TAG, "MQTT client stopped");
}

esp_err_t mqtt_manager_publish_reading(const temp_cube_bme280_reading_t *reading,
                                       uint32_t connect_timeout_ms)
{
    if (reading == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_client == NULL || s_mqtt_event_group == NULL) {
        ESP_LOGW(TAG, "Publish skipped, MQTT client is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_connected) {
        EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group, MQTT_CONNECTED_BIT,
                                               pdFALSE, pdFALSE,
                                               pdMS_TO_TICKS(connect_timeout_ms));
        if ((bits & MQTT_CONNECTED_BIT) == 0) {
            ESP_LOGW(TAG, "Publish skipped, broker connect timed out");
            return ESP_ERR_TIMEOUT;
        }
    }

    char payload[192];
    int len = snprintf(payload, sizeof(payload),
                       "{\"temperature_c\":%.2f,\"humidity_percent\":%.2f,"
                       "\"pressure_hpa\":%.2f,\"altitude_m\":%.2f}",
                       reading->temperature_c,
                       reading->humidity_percent,
                       reading->pressure_hpa,
                       reading->altitude_m);
    if (len < 0 || len >= (int)sizeof(payload)) {
        ESP_LOGE(TAG, "Reading payload did not fit in buffer");
        return ESP_ERR_NO_MEM;
    }

    int msg_id = esp_mqtt_client_publish(s_client, CONFIG_MQTT_READINGS_TOPIC,
                                         payload, len, CONFIG_MQTT_QOS, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Publish failed on [%s]", CONFIG_MQTT_READINGS_TOPIC);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Published reading to [%s] (msg_id=%d): %s",
             CONFIG_MQTT_READINGS_TOPIC, msg_id, payload);
    return ESP_OK;
}

esp_err_t mqtt_manager_wait_for_ota_update(char *url, size_t url_len,
                                           uint32_t timeout_ms)
{
    if (url == NULL || url_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_client == NULL || s_mqtt_event_group == NULL) {
        ESP_LOGW(TAG, "OTA wait skipped, MQTT client is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group, MQTT_OTA_UPDATE_BIT,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(timeout_ms));
    if ((bits & MQTT_OTA_UPDATE_BIT) == 0) {
        return ESP_ERR_TIMEOUT;
    }

    snprintf(url, url_len, "%s", s_ota_url);
    return ESP_OK;
}
