#include "temp_cube_bme280.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <esp_check.h>
#include <esp_log.h>
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <bme280.h>

#define TAG                         "temp_cube_bme280"
#define I2C_XFER_TIMEOUT_MS         1000

#define BME280_I2C_ADDR_PRIMARY     CONFIG_TEMP_CUBE_BME280_PRIMARY_I2C_ADDR
#define BME280_I2C_ADDR_SECONDARY   CONFIG_TEMP_CUBE_BME280_SECONDARY_I2C_ADDR
#define BME280_I2C_CLOCK_HZ         CONFIG_TEMP_CUBE_BME280_I2C_CLOCK_HZ
#define BME280_SEA_LEVEL_HPA        ((float)CONFIG_TEMP_CUBE_BME280_SEA_LEVEL_PRESSURE_PA / 100.0f)
#define BME280_UPDATE_INTERVAL_MS   CONFIG_TEMP_CUBE_BME280_UPDATE_INTERVAL_MS

struct temp_cube_bme280_t {
    i2c_master_dev_handle_t i2c_dev;
    bme280_data_t calib;
    int32_t t_fine;
};

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static int16_t read_s16_le(const uint8_t *data)
{
    return (int16_t)read_u16_le(data);
}

static esp_err_t read_bytes(temp_cube_bme280_handle_t handle, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(handle->i2c_dev, &reg, 1, data, len, I2C_XFER_TIMEOUT_MS);
}

static esp_err_t write_byte(temp_cube_bme280_handle_t handle, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = { reg, value };
    return i2c_master_transmit(handle->i2c_dev, data, sizeof(data), I2C_XFER_TIMEOUT_MS);
}

static esp_err_t read_calibration(temp_cube_bme280_handle_t handle)
{
    uint8_t tp[24];
    uint8_t h1;
    uint8_t h[7];

    ESP_RETURN_ON_ERROR(read_bytes(handle, BME280_REGISTER_DIG_T1, tp, sizeof(tp)), TAG, "BME280 T/P calibration read failed");
    ESP_RETURN_ON_ERROR(read_bytes(handle, BME280_REGISTER_DIG_H1, &h1, 1), TAG, "BME280 H1 calibration read failed");
    ESP_RETURN_ON_ERROR(read_bytes(handle, BME280_REGISTER_DIG_H2, h, sizeof(h)), TAG, "BME280 humidity calibration read failed");

    handle->calib.dig_t1 = read_u16_le(&tp[0]);
    handle->calib.dig_t2 = read_s16_le(&tp[2]);
    handle->calib.dig_t3 = read_s16_le(&tp[4]);
    handle->calib.dig_p1 = read_u16_le(&tp[6]);
    handle->calib.dig_p2 = read_s16_le(&tp[8]);
    handle->calib.dig_p3 = read_s16_le(&tp[10]);
    handle->calib.dig_p4 = read_s16_le(&tp[12]);
    handle->calib.dig_p5 = read_s16_le(&tp[14]);
    handle->calib.dig_p6 = read_s16_le(&tp[16]);
    handle->calib.dig_p7 = read_s16_le(&tp[18]);
    handle->calib.dig_p8 = read_s16_le(&tp[20]);
    handle->calib.dig_p9 = read_s16_le(&tp[22]);
    handle->calib.dig_h1 = h1;
    handle->calib.dig_h2 = read_s16_le(&h[0]);
    handle->calib.dig_h3 = h[2];
    handle->calib.dig_h4 = (int16_t)(((int16_t)(int8_t)h[3] << 4) | (h[4] & 0x0f));
    handle->calib.dig_h5 = (int16_t)(((int16_t)(int8_t)h[5] << 4) | (h[4] >> 4));
    handle->calib.dig_h6 = (int8_t)h[6];

    return ESP_OK;
}

static float altitude_from_pressure(float pressure_hpa)
{
    return 44330.0f * (1.0f - powf(pressure_hpa / BME280_SEA_LEVEL_HPA, 0.1903f));
}

esp_err_t temp_cube_bme280_init(i2c_master_bus_handle_t bus, temp_cube_bme280_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(bus && handle, ESP_ERR_INVALID_ARG, TAG, "invalid BME280 init arguments");

    uint8_t addr = BME280_I2C_ADDR_PRIMARY;
    esp_err_t ret = i2c_master_probe(bus, addr, I2C_XFER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        addr = BME280_I2C_ADDR_SECONDARY;
        ESP_RETURN_ON_ERROR(i2c_master_probe(bus, addr, I2C_XFER_TIMEOUT_MS), TAG, "BME280 not found on I2C bus");
    }

    temp_cube_bme280_handle_t sensor = calloc(1, sizeof(struct temp_cube_bme280_t));
    ESP_RETURN_ON_FALSE(sensor, ESP_ERR_NO_MEM, TAG, "no memory for BME280 handle");

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = BME280_I2C_CLOCK_HZ,
    };
    ESP_GOTO_ON_ERROR(i2c_master_bus_add_device(bus, &dev_cfg, &sensor->i2c_dev), err, TAG, "BME280 add device failed");

    uint8_t chip_id = 0;
    ESP_GOTO_ON_ERROR(read_bytes(sensor, BME280_REGISTER_CHIPID, &chip_id, 1), err, TAG, "BME280 chip id read failed");
    ESP_GOTO_ON_FALSE(chip_id == BME280_DEFAULT_CHIPID, ESP_ERR_INVALID_RESPONSE, err, TAG, "unexpected BME280 chip id: 0x%02x", chip_id);

    ESP_GOTO_ON_ERROR(write_byte(sensor, BME280_REGISTER_SOFTRESET, 0xb6), err, TAG, "BME280 reset failed");
    vTaskDelay(pdMS_TO_TICKS(300));

    uint8_t status = 0;
    do {
        ESP_GOTO_ON_ERROR(read_bytes(sensor, BME280_REGISTER_STATUS, &status, 1), err, TAG, "BME280 status read failed");
        vTaskDelay(pdMS_TO_TICKS(10));
    } while ((status & 0x01) != 0);

    ESP_GOTO_ON_ERROR(read_calibration(sensor), err, TAG, "BME280 calibration read failed");

    ESP_GOTO_ON_ERROR(write_byte(sensor, BME280_REGISTER_CONTROLHUMID, 0x05), err, TAG, "BME280 humidity sampling failed");
    ESP_GOTO_ON_ERROR(write_byte(sensor, BME280_REGISTER_CONFIG, 0x00), err, TAG, "BME280 config failed");
    ESP_GOTO_ON_ERROR(write_byte(sensor, BME280_REGISTER_CONTROL, 0xb7), err, TAG, "BME280 measurement config failed");

    *handle = sensor;
    ESP_LOGI(TAG, "BME280 initialized at I2C address 0x%02x", addr);
    return ESP_OK;

err:
    if (sensor->i2c_dev) {
        i2c_master_bus_rm_device(sensor->i2c_dev);
    }
    free(sensor);
    return ret;
}

esp_err_t temp_cube_bme280_read(temp_cube_bme280_handle_t handle, temp_cube_bme280_reading_t *reading)
{
    ESP_RETURN_ON_FALSE(handle && reading, ESP_ERR_INVALID_ARG, TAG, "invalid BME280 read arguments");

    uint8_t status = 0;
    do {
        ESP_RETURN_ON_ERROR(read_bytes(handle, BME280_REGISTER_STATUS, &status, 1), TAG, "BME280 status read failed");
        if (status & 0x08) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    } while (status & 0x08);

    uint8_t data[8];
    ESP_RETURN_ON_ERROR(read_bytes(handle, BME280_REGISTER_PRESSUREDATA, data, sizeof(data)), TAG, "BME280 measurement read failed");

    const int32_t adc_p = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);
    const int32_t adc_t = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | (data[5] >> 4);
    const int32_t adc_h = ((int32_t)data[6] << 8) | data[7];

    const bme280_data_t *calib = &handle->calib;
    int32_t var1 = ((((adc_t >> 3) - ((int32_t)calib->dig_t1 << 1))) * ((int32_t)calib->dig_t2)) >> 11;
    int32_t var2 = (((((adc_t >> 4) - ((int32_t)calib->dig_t1)) * ((adc_t >> 4) - ((int32_t)calib->dig_t1))) >> 12) *
                    ((int32_t)calib->dig_t3)) >> 14;
    handle->t_fine = var1 + var2;
    reading->temperature_c = ((handle->t_fine * 5 + 128) >> 8) / 100.0f;

    int64_t p_var1 = (int64_t)handle->t_fine - 128000;
    int64_t p_var2 = p_var1 * p_var1 * (int64_t)calib->dig_p6;
    p_var2 = p_var2 + ((p_var1 * (int64_t)calib->dig_p5) << 17);
    p_var2 = p_var2 + (((int64_t)calib->dig_p4) << 35);
    p_var1 = ((p_var1 * p_var1 * (int64_t)calib->dig_p3) >> 8) + ((p_var1 * (int64_t)calib->dig_p2) << 12);
    p_var1 = (((((int64_t)1) << 47) + p_var1)) * ((int64_t)calib->dig_p1) >> 33;
    if (p_var1 == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    int64_t pressure = 1048576 - adc_p;
    pressure = (((pressure << 31) - p_var2) * 3125) / p_var1;
    p_var1 = (((int64_t)calib->dig_p9) * (pressure >> 13) * (pressure >> 13)) >> 25;
    p_var2 = (((int64_t)calib->dig_p8) * pressure) >> 19;
    pressure = ((pressure + p_var1 + p_var2) >> 8) + (((int64_t)calib->dig_p7) << 4);
    reading->pressure_hpa = (float)(pressure >> 8) / 100.0f;

    int32_t h_var = handle->t_fine - 76800;
    h_var = (((((adc_h << 14) - (((int32_t)calib->dig_h4) << 20) - (((int32_t)calib->dig_h5) * h_var)) + 16384) >> 15) *
             (((((((h_var * ((int32_t)calib->dig_h6)) >> 10) *
                  (((h_var * ((int32_t)calib->dig_h3)) >> 11) + 32768)) >> 10) + 2097152) *
               ((int32_t)calib->dig_h2) + 8192) >> 14));
    h_var = h_var - (((((h_var >> 15) * (h_var >> 15)) >> 7) * ((int32_t)calib->dig_h1)) >> 4);
    h_var = h_var < 0 ? 0 : h_var;
    h_var = h_var > 419430400 ? 419430400 : h_var;
    reading->humidity_percent = (h_var >> 12) / 1024.0f;
    reading->altitude_m = altitude_from_pressure(reading->pressure_hpa);

    return ESP_OK;
}

uint32_t temp_cube_bme280_update_interval_ms(void)
{
    return BME280_UPDATE_INTERVAL_MS;
}
