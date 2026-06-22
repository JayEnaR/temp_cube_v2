idf_component_register(SRCS "mqtt-manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_event mqtt temp_cube_bme280)
