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

// Calibration buffer sizes derived from register map
#define BME280_CALIB_HUMI_SIZE_START    (1u)                                                        // dig_H1 at 0xA1 is a single byte
#define BME280_CALIB_HUMI_SIZE_CONTINUE (BME280_CALIB_HUMI_ADDR_END - BME280_CALIB_HUMI_ADDR_CONTINUE + 1u)  // dig_H2-H6 at 0xE1-0xE7
#define BME280_HUMIDITY_SIZE    (BME280_CALIB_HUMI_SIZE_START + BME280_CALIB_HUMI_SIZE_CONTINUE)    // total humidity calib bytes
#define BME280_PRESS_SIZE       (BME280_CALIB_PRES_ADDR_END - BME280_CALIB_PRES_ADDR_START + 1u)   // 18 bytes
#define BME280_TEMPERATURE_SIZE (BME280_CALIB_TEMP_ADDR_END - BME280_CALIB_TEMP_ADDR_START + 1u)   // 6 bytes

// Portable integer type aliases
typedef int8_t   bme280_int8_t;
typedef uint8_t  bme280_uint8_t;
typedef int16_t  bme280_int16_t;
typedef uint16_t bme280_uint16_t;
typedef int32_t  bme280_int32_t;
typedef uint32_t bme280_uint32_t;
typedef int64_t  bme280_int64_t;

static const uint8_t BME280_RESET = 0xB6;  // soft reset command value written to BME280_RESET_ADDR
static const uint8_t BME280_ID    = 0x60;  // expected chip ID read from BME280_ID_ADDR

typedef enum {
    BME280_STATUS_OK,                // operation completed successfully
    BME280_STATUS_RESET,             // device was reset
    BME280_STATUS_ERROR,             // generic unspecified error
    BME280_STATUS_NULL_PTR,          // a required pointer argument was NULL
    BME280_STATUS_INVALID_ARG,       // argument was non-null but had an invalid value
    BME280_STATUS_INVALID_STATE,     // device is in wrong state for this operation
    BME280_STATUS_TIMEOUT,           // forced mode measurement did not complete within TIMEOUT iterations
    BME280_STATUS_COMM_ERROR,        // I2C/SPI read or write returned a failure
    BME280_STATUS_DEVICE_NOT_FOUND   // chip ID read succeeded but value did not match 0x60
} bme280_status_t;

typedef enum {
    SLEEP_MODE  = (0b00 << BME280_MODE),  // no measurements, lowest power
    FORCED_MODE = (0b01 << BME280_MODE),  // single measurement then returns to sleep
    NORMAL_MODE = (0b11 << BME280_MODE)   // continuous measurements at standby interval
} bme280_mode_t;

typedef enum {
    BME280_STANDBY_0_5  = (0b000 << BME280_CONFIG_STANDBY),  // 0.5 ms
    BME280_STANDBY_62_5 = (0b001 << BME280_CONFIG_STANDBY),  // 62.5 ms
    BME280_STANDBY_125  = (0b010 << BME280_CONFIG_STANDBY),  // 125 ms
    BME280_STANDBY_250  = (0b011 << BME280_CONFIG_STANDBY),  // 250 ms
    BME280_STANDBY_500  = (0b100 << BME280_CONFIG_STANDBY),  // 500 ms
    BME280_STANDBY_1000 = (0b101 << BME280_CONFIG_STANDBY),  // 1000 ms
    BME280_STANDBY_10   = (0b110 << BME280_CONFIG_STANDBY),  // 10 ms
    BME280_STANDBY_20   = (0b111 << BME280_CONFIG_STANDBY)   // 20 ms
} bme280_standby_t;

typedef enum {
    BME280_FILTER_OFF = (0b000 << BME280_CONFIG_FILTER),  // IIR filter disabled
    BME280_FILTER_2   = (0b001 << BME280_CONFIG_FILTER),  // coefficient 2
    BME280_FILTER_4   = (0b010 << BME280_CONFIG_FILTER),  // coefficient 4
    BME280_FILTER_8   = (0b011 << BME280_CONFIG_FILTER),  // coefficient 8
    BME280_FILTER_16  = (0b100 << BME280_CONFIG_FILTER)   // coefficient 16 (slowest, smoothest)
} bme280_filter_t;

typedef enum {
    BME280_SPI_DISABLE = (0b000 << BME280_CONFIG_SPI_STATUS),  // SPI 4-wire (default)
    BME280_SPI_ENABLE  = (0b001 << BME280_CONFIG_SPI_STATUS)   // SPI 3-wire
} bme280_spi_status_t;

typedef enum {
    BME280_HUMIDITY_OVERSAMPLING_SKIPPED = (0b000 << BME280_HUMI_OVS),  // output 0x8000
    BME280_HUMIDITY_OVERSAMPLING_X1      = (0b001 << BME280_HUMI_OVS),
    BME280_HUMIDITY_OVERSAMPLING_X2      = (0b010 << BME280_HUMI_OVS),
    BME280_HUMIDITY_OVERSAMPLING_X4      = (0b011 << BME280_HUMI_OVS),
    BME280_HUMIDITY_OVERSAMPLING_X8      = (0b100 << BME280_HUMI_OVS),
    BME280_HUMIDITY_OVERSAMPLING_X16     = (0b101 << BME280_HUMI_OVS)
} bme280_humidity_ovs_t;

typedef enum {
    BME280_PRESSURE_OVERSAMPLING_SKIPPED = (0b000 << BME280_PRES_OVS),  // output 0x80000
    BME280_PRESSURE_OVERSAMPLING_X1      = (0b001 << BME280_PRES_OVS),
    BME280_PRESSURE_OVERSAMPLING_X2      = (0b010 << BME280_PRES_OVS),
    BME280_PRESSURE_OVERSAMPLING_X4      = (0b011 << BME280_PRES_OVS),
    BME280_PRESSURE_OVERSAMPLING_X8      = (0b100 << BME280_PRES_OVS),
    BME280_PRESSURE_OVERSAMPLING_X16     = (0b101 << BME280_PRES_OVS)
} bme280_pressure_ovs_t;

typedef enum {
    BME280_TEMPERATURE_OVERSAMPLING_SKIPPED = (0b000 << BME280_TEMP_OVS),  // output 0x80000
    BME280_TEMPERATURE_OVERSAMPLING_X1      = (0b001 << BME280_TEMP_OVS),
    BME280_TEMPERATURE_OVERSAMPLING_X2      = (0b010 << BME280_TEMP_OVS),
    BME280_TEMPERATURE_OVERSAMPLING_X4      = (0b011 << BME280_TEMP_OVS),
    BME280_TEMPERATURE_OVERSAMPLING_X8      = (0b100 << BME280_TEMP_OVS),
    BME280_TEMPERATURE_OVERSAMPLING_X16     = (0b101 << BME280_TEMP_OVS)
} bme280_temperature_ovs_t;

// Factory-programmed calibration coefficients read from the sensor at startup
typedef struct {
    bme280_uint16_t dig_T1;  // temperature: unsigned
    bme280_int16_t  dig_T2;
    bme280_int16_t  dig_T3;

    bme280_uint16_t dig_P1;  // pressure: P1 unsigned, P2-P9 signed
    bme280_int16_t  dig_P2;
    bme280_int16_t  dig_P3;
    bme280_int16_t  dig_P4;
    bme280_int16_t  dig_P5;
    bme280_int16_t  dig_P6;
    bme280_int16_t  dig_P7;
    bme280_int16_t  dig_P8;
    bme280_int16_t  dig_P9;

    bme280_uint8_t dig_H1;  // humidity: H1/H3 unsigned, H2/H4/H5 signed, H4/H5 non-trivially packed
    bme280_int16_t dig_H2;
    bme280_uint8_t dig_H3;
    bme280_int16_t dig_H4;
    bme280_int16_t dig_H5;
    bme280_int8_t  dig_H6;
} bme280_calibration_data_t;

// Raw 20-bit (pressure/temperature) and 16-bit (humidity) ADC values from registers 0xF7-0xFE
typedef struct {
    bme280_uint16_t bme280_humidity_raw;
    bme280_uint32_t bme280_pressure_raw;
    bme280_uint32_t bme280_temperature_raw;
} bme280_raw_t;

// Contents of register 0xF5 (config): standby time, IIR filter, SPI mode
typedef struct {
    bme280_standby_t    bme280_standby;
    bme280_filter_t     bme280_filter;
    bme280_spi_status_t bme280_spi_status;
} bme280_config_t;

// Platform-provided I2C/SPI read: returns BME280_STATUS_OK on success
typedef bme280_status_t (*bme280_read_fn_t)(
        uint8_t dev_addr,
        uint8_t reg_addr,
        uint8_t* data,
        uint8_t len,
        void* ctx
);

// Platform-provided I2C/SPI write: returns BME280_STATUS_OK on success
typedef bme280_status_t (*bme280_write_fn_t)(
        uint8_t dev_addr,
        uint8_t reg_addr,
        uint8_t* data,
        uint8_t len,
        void* ctx
);

// HAL-independent bus abstraction: fill this with platform callbacks before calling bme280_init
typedef struct {
    bme280_read_fn_t  read;      // pointer to platform read function
    bme280_write_fn_t write;     // pointer to platform write function
    void*             ctx;       // passed as-is to read/write (e.g. &hi2c1)
    uint8_t           dev_addr;  // 8-bit I2C address (7-bit addr << 1)
} bme280_bus_t;

// Main device handle: initialize to zero, fill mode/oversampling fields, then call bme280_init
typedef struct {
    bme280_bus_t bme280_bus;  // bus callbacks and address

    bme280_status_t  status;  // last recorded driver status
    bme280_uint8_t   reset;   // set to BME280_RESET after soft reset

    bme280_mode_t           bme280_mode;            // SLEEP / FORCED / NORMAL
    bme280_humidity_ovs_t   bme280_humidity_ovs;    // ctrl_hum oversampling
    bme280_pressure_ovs_t   bme280_pressure_ovs;    // ctrl_meas pressure oversampling
    bme280_temperature_ovs_t bme280_temperature_ovs; // ctrl_meas temperature oversampling

    uint8_t bme280_calib_buf[BME280_HUMIDITY_SIZE + BME280_PRESS_SIZE + BME280_TEMPERATURE_SIZE];  // raw calib bytes
    bme280_calibration_data_t bme280_calib_data;  // parsed calibration coefficients
    bme280_raw_t              bme280_raw;          // last raw ADC values from bme280_read_raw
} bme280_t;

// Setup
bme280_status_t bme280_init(bme280_t* dev, const bme280_bus_t* bus);          // validates bus, copies it, reads and verifies chip ID
bme280_status_t bme280_configure(bme280_t* dev, const bme280_config_t* cfg);  // writes 0xF5, 0xF2, 0xF4 in correct order
bme280_status_t bme280_reset(bme280_t* dev);                                   // clears device state and sends soft reset
bme280_status_t bme280_set_mode(bme280_t* dev, bme280_mode_t mode);            // read-modify-write on mode bits [1:0] of 0xF4

// Data acquisition
bme280_status_t bme280_read_calibration(bme280_t* dev);                        // reads calib registers and parses into calib_data; call once at init
bme280_status_t bme280_read_raw(bme280_t* dev);                                // triggers measurement (forced) or reads result (normal), stores in bme280_raw
bme280_status_t bme280_read(bme280_t* dev, bme280_int32_t* T, bme280_uint32_t* H, bme280_uint32_t* P);        // fixed-point: T in degC, H in %RH, P in Pa
bme280_status_t bme280_read_double(bme280_t* dev, double* T, double* H, double* P);                            // floating-point: T in degC, H in %RH, P in Pa

// Preset configurations (fill dev and cfg, then call bme280_configure)
bme280_status_t bme280_weatherMonitoring(bme280_t* dev, bme280_config_t* cfg);  // forced, 1x all, no filter
bme280_status_t bme280_humiditySensing(bme280_t* dev, bme280_config_t* cfg);    // forced, humidity only
bme280_status_t bme280_indoorNavigation(bme280_t* dev, bme280_config_t* cfg);   // normal, high oversampling, filter x16
bme280_status_t bme280_gaming(bme280_t* dev, bme280_config_t* cfg);             // normal, pressure x4, filter x16


#endif /* BME280_BME280_H_ */
