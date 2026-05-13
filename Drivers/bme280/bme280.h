/*
 * bme280.h
 *
 *  Created on: May 4, 2026
 *      Author: p.baranchuk
 */

#ifndef BME280_BME280_H_
#define BME280_BME280_H_

#include "bme280_regs.h"
#include "stdint.h"
#include "stddef.h"

#define BME280_CALIB_HUMI_SIZE_START     (1u)
#define BME280_CALIB_HUMI_SIZE_CONTINUE  (BME280_CALIB_HUMI_ADDR_END - BME280_CALIB_HUMI_ADDR_CONTINUE + 1u)
#define BME280_HUMIDITY_SIZE (BME280_CALIB_HUMI_SIZE_START + BME280_CALIB_HUMI_SIZE_CONTINUE)
#define BME280_PRESS_SIZE (BME280_CALIB_PRES_ADDR_END - BME280_CALIB_PRES_ADDR_START + 1u)
#define BME280_TEMPERATURE_SIZE (BME280_CALIB_TEMP_ADDR_END - BME280_CALIB_TEMP_ADDR_START + 1u)

typedef int8_t bme280_int8_t;
typedef uint8_t bme280_uint8_t;
typedef int16_t bme280_int16_t;
typedef uint16_t bme280_uint16_t;
typedef int32_t bme280_int32_t;
typedef uint32_t bme280_uint32_t;
typedef int64_t bme280_int64_t;

static const uint8_t BME280_RESET = 0xB6;
static const uint8_t BME280_ID = 0x60;

typedef enum {
    BME280_STATUS_OK,
    BME280_STATUS_RESET,
    BME280_STATUS_ERROR,
    BME280_STATUS_INVALID_ARG,
    BME280_STATUS_INVALID_STATE
} bme280_status_t;

typedef enum {
    SLEEP_MODE = (0b00 << BME280_MODE),
    FORCED_MODE = (0b01 << BME280_MODE),
    NORMAL_MODE = (0b11 << BME280_MODE)
} bme280_mode_t;

typedef enum {
    BME280_STANDBY_0_5 = (0b000 << BME280_CONFIG_STANDBY),
    BME280_STANDBY_62_5 = (0b001 << BME280_CONFIG_STANDBY),
    BME280_STANDBY_125 = (0b010 << BME280_CONFIG_STANDBY),
    BME280_STANDBY_250 = (0b011 << BME280_CONFIG_STANDBY),
    BME280_STANDBY_500 = (0b100 << BME280_CONFIG_STANDBY),
    BME280_STANDBY_1000 = (0b101 << BME280_CONFIG_STANDBY),
    BME280_STANDBY_10 = (0b110 << BME280_CONFIG_STANDBY),
    BME280_STANDBY_20 = (0b111 << BME280_CONFIG_STANDBY)
} bme280_standby_t;

typedef enum {
    BME280_FILTER_OFF = (0b000 << BME280_CONFIG_FILTER),
    BME280_FILTER_2 = (0b001 << BME280_CONFIG_FILTER),
    BME280_FILTER_4 = (0b010 << BME280_CONFIG_FILTER),
    BME280_FILTER_8 = (0b011 << BME280_CONFIG_FILTER),
    BME280_FILTER_16 = (0b100 << BME280_CONFIG_FILTER)
} bme280_filter_t;

typedef enum {
    BME280_SPI_DISABLE = (0b000 << BME280_CONFIG_SPI_STATUS),
    BME280_SPI_ENABLE = (0b001 << BME280_CONFIG_SPI_STATUS)
} bme280_spi_status_t;

typedef enum {
    BME280_HUMIDITY_OVERSAMPLING_SKIPPED = (0b000 << BME280_HUMI_OVS),
    BME280_HUMIDITY_OVERSAMPLING_X1 = (0b001 << BME280_HUMI_OVS),
    BME280_HUMIDITY_OVERSAMPLING_X2 = (0b010 << BME280_HUMI_OVS),
    BME280_HUMIDITY_OVERSAMPLING_X4 = (0b011 << BME280_HUMI_OVS),
    BME280_HUMIDITY_OVERSAMPLING_X8 = (0b100 << BME280_HUMI_OVS),
    BME280_HUMIDITY_OVERSAMPLING_X16 = (0b101 << BME280_HUMI_OVS)
} bme280_humidity_ovs_t;

typedef enum {
    BME280_PRESSURE_OVERSAMPLING_SKIPPED = (0b000 << BME280_PRES_OVS),
    BME280_PRESSURE_OVERSAMPLING_X1 = (0b001 << BME280_PRES_OVS),
    BME280_PRESSURE_OVERSAMPLING_X2 = (0b010 << BME280_PRES_OVS),
    BME280_PRESSURE_OVERSAMPLING_X4 = (0b011 << BME280_PRES_OVS),
    BME280_PRESSURE_OVERSAMPLING_X8 = (0b100 << BME280_PRES_OVS),
    BME280_PRESSURE_OVERSAMPLING_X16 = (0b101 << BME280_PRES_OVS)
} bme280_pressure_ovs_t;

typedef enum {
    BME280_TEMPERATURE_OVERSAMPLING_SKIPPED = (0b000 << BME280_TEMP_OVS),
    BME280_TEMPERATURE_OVERSAMPLING_X1 = (0b001 << BME280_TEMP_OVS),
    BME280_TEMPERATURE_OVERSAMPLING_X2 = (0b010 << BME280_TEMP_OVS),
    BME280_TEMPERATURE_OVERSAMPLING_X4 = (0b011 << BME280_TEMP_OVS),
    BME280_TEMPERATURE_OVERSAMPLING_X8 = (0b100 << BME280_TEMP_OVS),
    BME280_TEMPERATURE_OVERSAMPLING_X16 = (0b101 << BME280_TEMP_OVS)
} bme280_temperature_ovs_t;

typedef struct {
    bme280_uint16_t dig_T1;
    bme280_int16_t dig_T2;
    bme280_int16_t dig_T3;

    bme280_uint16_t dig_P1;
    bme280_int16_t dig_P2;
    bme280_int16_t dig_P3;
    bme280_int16_t dig_P4;
    bme280_int16_t dig_P5;
    bme280_int16_t dig_P6;
    bme280_int16_t dig_P7;
    bme280_int16_t dig_P8;
    bme280_int16_t dig_P9;

    bme280_uint8_t dig_H1;
    bme280_int16_t dig_H2;
    bme280_uint8_t dig_H3;
    bme280_int16_t dig_H4;
    bme280_int16_t dig_H5;
    bme280_int8_t dig_H6;
} bme280_calibration_data_t;

typedef struct {
    bme280_uint16_t bme280_humidity_raw;
    bme280_uint32_t bme280_pressure_raw;
    bme280_uint32_t bme280_temperature_raw;
} bme280_raw_t;

typedef struct {
    bme280_standby_t bme280_standby;
    bme280_filter_t bme280_filter;
    bme280_spi_status_t bme280_spi_status;
} bme280_config_t;

typedef bme280_status_t (*bme280_read_fn_t)(
        uint8_t dev_addr,
        uint8_t reg_addr,
        uint8_t* data,
        uint8_t len,
        void* ctx
);

typedef bme280_status_t (*bme280_write_fn_t)(
        uint8_t dev_addr,
        uint8_t reg_addr,
        const uint8_t* data,
        uint8_t len,
        void* ctx
);

typedef struct {
    bme280_read_fn_t read;
    bme280_write_fn_t write;
    void* ctx;
    uint8_t dev_addr;
} bme280_bus_t;

typedef struct {
    bme280_bus_t bme280_bus;

    bme280_status_t status;
    bme280_uint8_t reset;

    bme280_mode_t bme280_mode;
    bme280_humidity_ovs_t bme280_humidity_ovs;
    bme280_pressure_ovs_t bme280_pressure_ovs;
    bme280_temperature_ovs_t bme280_temperature_ovs;

    uint8_t bme280_calib_buf[BME280_HUMIDITY_SIZE + BME280_PRESS_SIZE + BME280_TEMPERATURE_SIZE];
    bme280_calibration_data_t bme280_calib_data;
    bme280_raw_t bme280_raw;
} bme280_t;

bme280_status_t bme280_init(bme280_t* dev, const bme280_bus_t* bus);
bme280_status_t bme280_reset(bme280_t* dev);
bme280_status_t bme280_set_mode(bme280_t* dev, bme280_mode_t mode);

bme280_status_t bme280_read_calibration(bme280_t* dev);
bme280_status_t bme280_set_config(bme280_t* dev, bme280_config_t cfg);
bme280_status_t bme280_read_raw(bme280_t* dev);
bme280_status_t bme280_read(bme280_t* dev, bme280_uint32_t* T, bme280_uint32_t* H, bme280_uint32_t* P);
bme280_status_t bme280_convertCalibData(bme280_t* dev);

bme280_status_t bme280_weatherMonitoring(bme280_t* dev, bme280_config_t* cfg);
bme280_status_t bme280_humiditySensing(bme280_t* dev, bme280_config_t* cfg);
bme280_status_t bme280_indoorNavigation(bme280_t* dev, bme280_config_t* cfg);
bme280_status_t bme280_gaming(bme280_t* dev, bme280_config_t* cfg);


#endif /* BME280_BME280_H_ */
