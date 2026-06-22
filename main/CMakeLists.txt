idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES ssd1306 esp_driver_i2c temp_cube_bme280 wifi-manager button mqtt-manager)
