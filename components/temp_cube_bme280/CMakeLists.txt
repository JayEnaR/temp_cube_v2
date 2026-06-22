idf_component_register(SRCS "temp_cube_bme280.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_i2c bme280)
