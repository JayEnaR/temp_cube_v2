idf_component_register(SRCS "ota-manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES app_update esp_https_ota esp_http_client esp-tls nvs_flash)
