idf_component_register(SRCS "soft-ap.c"
                        INCLUDE_DIRS "include"
                        REQUIRES esp_wifi esp_netif nvs_flash esp_event http-server)
