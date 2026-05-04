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

typedef uint8_t bme280_uint8_t;
typedef int16_t bme280_int16_t;
typedef uint16_t bme280_uint16_t;
typedef int32_t bme280_int32_t;
typedef uint32_t bme280_uint32_t;

const uint8_t BME280_RESET = 0xB6;

typedef enum {
    BME280_STATUS_OK,
    BME280_STATUS_RESET,
    BME280_STATUS_ERROR
} bme280_status_t;

typedef enum {
    SLEEP_MODE = 0b00,
    FORCED_MODE = 0b01,
    NORMAL_MODE = 0b11
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
    BME280_FILTER_OFF = (0b000 << BME280_CONFIG_FILER),
    BME280_FILTER_2 = (0b001 << BME280_CONFIG_FILER),
    BME280_FILTER_4 = (0b010 << BME280_CONFIG_FILER),
    BME280_FILTER_8 = (0b011 << BME280_CONFIG_FILER),
    BME280_FILTER_16 = (0b100 << BME280_CONFIG_FILER)
} bme280_filter_t;

typedef enum {
    BME280_SPI_DISABLE = (0b000 << BME280_CONFIG_SPI_STATUS),
    BME280_SPI_ENABLE = (0b001 << BME280_CONFIG_SPI_STATUS)
} bme280_spi_status_t;

typedef enum {
    BME280_HUMIDITY_OVERSAMPLING_SKIPPED = 0b000,
    BME280_HUMIDITY_OVERSAMPLING_X1 = 0b001,
    BME280_HUMIDITY_OVERSAMPLING_X2 = 0b010,
    BME280_HUMIDITY_OVERSAMPLING_X4 = 0b011,
    BME280_HUMIDITY_OVERSAMPLING_X8 = 0b100,
    BME280_HUMIDITY_OVERSAMPLING_X16 = 0b101,
} bme280_humidity_ovs_t;

typedef enum {
    BME280_PRESSURE_OVERSAMPLING_SKIPPED = 0b000,
    BME280_PRESSURE_OVERSAMPLING_X1 = 0b001,
    BME280_PRESSURE_OVERSAMPLING_X2 = 0b010,
    BME280_PRESSURE_OVERSAMPLING_X4 = 0b011,
    BME280_PRESSURE_OVERSAMPLING_X8 = 0b100,
    BME280_PRESSURE_OVERSAMPLING_X16 = 0b101,
} bme280_pressure_ovs_t;

typedef enum {
    BME280_TEMPERATURE_OVERSAMPLING_SKIPPED = 0b000,
    BME280_TEMPEREATURE_OVERSAMPLING_X1 = 0b001,
    BME280_TEMPERATURE_OVERSAMPLING_X2 = 0b010,
    BME280_TEMPERATURE_OVERSAMPLING_X4 = 0b011,
    BME280_TEMPERATURE_OVERSAMPLING_X8 = 0b100,
    BME280_TEMPERATURE_OVERSAMPLING_X16 = 0b101,
} bme280_temperature_ovs_t;

typedef struct {
    bme280_status_t status;
    bme280_uint8_t reset;

    bme280_humidity_ovs_t bme280_humidity_ovs;
    bme280_pressure_ovs_t bme280_pressure_ovs;
    bme280_temperature_ovs_t bme280_temperature_ovs;
} bme280_t;

typedef struct {
    bme280_standby_t bme280_standby;
    bme280_filter_t bme280_filter;
    bme280_spi_status_t bme280_spi_status;
} bme280_config_t;

typedef struct {
    bme280_uint16_t bme280_humidity_raw;
    bme280_uint32_t bme280_pressure_raw;
    bme280_uint32_t bme280_temperature_raw;
} bme280_raw_t;

bme280_status_t bme280_init(bme280_t* dev, const bme280_bus_t* bus);
bme280_status_t bme280_reset(bme280_t* dev);

bme280_status_t bme280_read_calibration(bme280_t* dev);
bme280_status_t bme280_set_config(bme280_t* dev, bme280_config_t cfg);
bme280_status_t bme280_read_raw(bme280_t* dev, bme280_raw_t* raw);
bme280_status_t bme280_read(bme280_t* dev, float* T, float* H, float* P);

#endif /* BME280_BME280_H_ */
