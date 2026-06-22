#ifndef TEMP_CUBE_BME280_H
#define TEMP_CUBE_BME280_H

#include <stdint.h>
#include <esp_err.h>
#include <driver/i2c_master.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct temp_cube_bme280_t *temp_cube_bme280_handle_t;

typedef struct {
    float temperature_c;
    float humidity_percent;
    float pressure_hpa;
    float altitude_m;
} temp_cube_bme280_reading_t;

esp_err_t temp_cube_bme280_init(i2c_master_bus_handle_t bus, temp_cube_bme280_handle_t *handle);
esp_err_t temp_cube_bme280_read(temp_cube_bme280_handle_t handle, temp_cube_bme280_reading_t *reading);
uint32_t temp_cube_bme280_update_interval_ms(void);

#ifdef __cplusplus
}
#endif

#endif
