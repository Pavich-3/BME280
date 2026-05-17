/*
 * bme280.c
 *
 *  Created on: May 4, 2026
 *      Author: p.baranchuk
 */
#include "bme280.h"
#include "string.h"

#define TIMEOUT 1000u  // max polling iterations before read_raw returns error

static bme280_status_t bme280_convertCalibData(bme280_t* dev);

// Validates bus function pointers, copies bus into device struct, reads and verifies chip ID (0x60)
bme280_status_t bme280_init(bme280_t* dev, const bme280_bus_t* bus) {
    if (!dev || !bus) { return BME280_STATUS_NULL_PTR; }
    if (!bus->read || !bus->write) { return BME280_STATUS_INVALID_ARG; }

    uint8_t currentID = 0;
    dev->status = BME280_STATUS_OK;
    dev->bme280_bus = *bus;  // copy bus into device struct

    if (bus->read(bus->dev_addr, BME280_ID_ADDR, &currentID, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_COMM_ERROR;
    }
    if (currentID != BME280_ID) { return BME280_STATUS_DEVICE_NOT_FOUND; }  // 0x60 expected

    return BME280_STATUS_OK;
}

// Enters sleep, then writes 0xF5 (config), 0xF2 (ctrl_hum), 0xF4 (ctrl_meas) in datasheet-required order
bme280_status_t bme280_configure(bme280_t* dev, const bme280_config_t* cfg) {
    if (!dev || !cfg) { return BME280_STATUS_NULL_PTR; }
    if (!dev->bme280_bus.read || !dev->bme280_bus.write) { return BME280_STATUS_INVALID_STATE; }
    bme280_bus_t* bus = &dev->bme280_bus;

    // Inline sleep: read ctrl_meas, clear mode bits [1:0], write back
    // Avoids calling bme280_set_mode which would corrupt dev->bme280_mode before we write it at the end
    uint8_t ctrlMeas = 0;
    if (bus->read(bus->dev_addr, BME280_CTRL_MEAS_ADDR, &ctrlMeas, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_COMM_ERROR;
    }
    ctrlMeas &= ~0x03u;  // clear mode bits only, preserve osrs bits [7:2]
    if (bus->write(bus->dev_addr, BME280_CTRL_MEAS_ADDR, &ctrlMeas, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_COMM_ERROR;
    }

    uint8_t configReg = (cfg->bme280_standby | cfg->bme280_filter | cfg->bme280_spi_status);  // 0xF5
    if (bus->write(bus->dev_addr, BME280_CONFIG_ADDR, &configReg, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_COMM_ERROR;
    }
    configReg = (uint8_t)dev->bme280_humidity_ovs;  // 0xF2 must be written before 0xF4 to take effect
    if (bus->write(bus->dev_addr, BME280_CTRL_HUM_ADDR, &configReg, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_COMM_ERROR;
    }
    configReg = (uint8_t)((uint8_t)dev->bme280_temperature_ovs | (uint8_t)dev->bme280_pressure_ovs | (uint8_t)dev->bme280_mode);  // 0xF4
    if (bus->write(bus->dev_addr, BME280_CTRL_MEAS_ADDR, &configReg, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_COMM_ERROR;
    }

    return BME280_STATUS_OK;
}

// Saves bus, wipes device struct via memset, then sends soft reset command 0xB6 to register 0xE0
bme280_status_t bme280_reset(bme280_t* dev) {
    if (!dev) { return BME280_STATUS_NULL_PTR; }
    if (!dev->bme280_bus.write) { return BME280_STATUS_INVALID_STATE; }

    bme280_bus_t bme280_bus = dev->bme280_bus;  // save bus before memset clears it
    memset(dev, 0, sizeof(*dev));               // clear all device state
    dev->reset = BME280_RESET;
    dev->bme280_bus = bme280_bus;               // restore bus

    uint8_t resetCmd = BME280_RESET;  // local copy required: write fn takes non-const uint8_t*
    if (dev->bme280_bus.write(dev->bme280_bus.dev_addr, BME280_RESET_ADDR, &resetCmd, 1, dev->bme280_bus.ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_COMM_ERROR;
    }

    return BME280_STATUS_OK;
}

// Read-modify-write on 0xF4: patches mode bits [1:0] only, preserves osrs bits [7:2]
bme280_status_t bme280_set_mode(bme280_t* dev, bme280_mode_t mode) {
    if (!dev) { return BME280_STATUS_NULL_PTR; }
    if (!dev->bme280_bus.read || !dev->bme280_bus.write) { return BME280_STATUS_INVALID_STATE; }

    bme280_bus_t* bus = &dev->bme280_bus;

    uint8_t ctrlMeas = 0;
    if (bus->read(bus->dev_addr, BME280_CTRL_MEAS_ADDR, &ctrlMeas, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_COMM_ERROR;
    }
    ctrlMeas = ((ctrlMeas & 0xFC) | (mode & 0x03));  // clear [1:0], set new mode
    if (bus->write(bus->dev_addr, BME280_CTRL_MEAS_ADDR, &ctrlMeas, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_COMM_ERROR;
    }

    dev->bme280_mode = mode;  // keep struct in sync with hardware
    return BME280_STATUS_OK;
}

// Reads calibration registers in three bursts (0x88-0x9F, 0xA1, 0xE1-0xE7), then parses into calib_data
bme280_status_t bme280_read_calibration(bme280_t* dev) {
    if (!dev) { return BME280_STATUS_NULL_PTR; }
    if (!dev->bme280_bus.read || !dev->bme280_bus.write) { return BME280_STATUS_INVALID_STATE; }

    // First burst: temperature + pressure calibration (continuous block 0x88-0x9F)
    uint8_t len = BME280_CALIB_PRES_ADDR_END - BME280_CALIB_TEMP_ADDR_START + 1;  // 24 bytes
    if (dev->bme280_bus.read(dev->bme280_bus.dev_addr, BME280_CALIB_TEMP_ADDR_START, dev->bme280_calib_buf, len, dev->bme280_bus.ctx) != BME280_STATUS_OK) {
        dev->status = BME280_STATUS_COMM_ERROR;
        return BME280_STATUS_COMM_ERROR;
    }

    // Second burst: dig_H1 at 0xA1 (1 byte, separate from the humidity continue block)
    if (dev->bme280_bus.read(dev->bme280_bus.dev_addr, BME280_CALIB_HUMI_ADDR_START, (dev->bme280_calib_buf + len), BME280_CALIB_HUMI_SIZE_START, dev->bme280_bus.ctx) != BME280_STATUS_OK) {
        dev->status = BME280_STATUS_COMM_ERROR;
        return BME280_STATUS_COMM_ERROR;
    }
    len += BME280_CALIB_HUMI_SIZE_START;  // advance buffer offset past dig_H1

    // Third burst: dig_H2-H6 at 0xE1-0xE7
    if (dev->bme280_bus.read(dev->bme280_bus.dev_addr, BME280_CALIB_HUMI_ADDR_CONTINUE, (dev->bme280_calib_buf + len), (BME280_CALIB_HUMI_ADDR_END - BME280_CALIB_HUMI_ADDR_CONTINUE + 1), dev->bme280_bus.ctx) != BME280_STATUS_OK) {
        dev->status = BME280_STATUS_COMM_ERROR;
        return BME280_STATUS_COMM_ERROR;
    }

    if (bme280_convertCalibData(dev) != BME280_STATUS_OK) {
        dev->status = BME280_STATUS_ERROR;
        return BME280_STATUS_ERROR;
    }

    return BME280_STATUS_OK;
}

// Triggers single measurement (forced mode only), polls status bit 3, then reads 8 raw bytes from 0xF7-0xFE
bme280_status_t bme280_read_raw(bme280_t* dev) {
    if (!dev) { return BME280_STATUS_NULL_PTR; }
    if (!dev->bme280_bus.read || !dev->bme280_bus.write) { return BME280_STATUS_INVALID_STATE; }
    bme280_bus_t* bus = &dev->bme280_bus;

    uint32_t timeout = TIMEOUT;
    if (dev->bme280_mode == FORCED_MODE) {
        if (bme280_set_mode(dev, FORCED_MODE) != BME280_STATUS_OK) { return BME280_STATUS_COMM_ERROR; }  // kick off single measurement

        uint8_t status = 0;
        do {
            if (--timeout == 0) { return BME280_STATUS_TIMEOUT; }  // sensor not responding within TIMEOUT iterations

            if (bus->read(bus->dev_addr, BME280_STATUS_ADDR, &status, 1, bus->ctx) != BME280_STATUS_OK) {
                dev->status = BME280_STATUS_COMM_ERROR;
                return BME280_STATUS_COMM_ERROR;
            }
        } while (status & (1u << BME280_STATUS_MEASURING));  // bit 3: 1 = measurement in progress
    }

    // Burst read all measurement registers in one transaction for data consistency
    const uint8_t len = BME280_HUMI_LSB_ADDR - BME280_PRES_MSB_ADDR + 1;  // 8 bytes: 0xF7-0xFE
    uint8_t bme280_data_raw[len];
    if (bus->read(bus->dev_addr, BME280_PRES_MSB_ADDR, bme280_data_raw, len, bus->ctx) != BME280_STATUS_OK) {
        dev->status = BME280_STATUS_COMM_ERROR;
        return BME280_STATUS_COMM_ERROR;
    }

    // Assemble 20-bit ADC values: [MSB<<12 | LSB<<4 | XLSB>>4], cast to uint32_t before shifting to avoid overflow
    dev->bme280_raw.bme280_pressure_raw    = (bme280_uint32_t)(((uint32_t)bme280_data_raw[0] << 12) | ((uint32_t)bme280_data_raw[1] << 4) | (bme280_data_raw[2] >> 4));
    dev->bme280_raw.bme280_temperature_raw = (bme280_uint32_t)(((uint32_t)bme280_data_raw[3] << 12) | ((uint32_t)bme280_data_raw[4] << 4) | (bme280_data_raw[5] >> 4));
    dev->bme280_raw.bme280_humidity_raw    = (bme280_uint16_t)(((uint16_t)bme280_data_raw[6] << 8) | bme280_data_raw[7]);  // 16-bit, no XLSB byte

    return BME280_STATUS_OK;
}

// Parses raw calib_buf bytes into calibration_data struct using datasheet Table 16/17 register layout
static bme280_status_t bme280_convertCalibData(bme280_t* dev) {
    bme280_calibration_data_t* calib_data = &dev->bme280_calib_data;

    uint8_t* temp_calib_buf = dev->bme280_calib_buf;                                               // offset 0: starts at 0x88
    uint8_t* pres_calib_buf = dev->bme280_calib_buf + BME280_TEMPERATURE_SIZE;                     // offset 6: starts at 0x8E
    uint8_t* humi_calib_buf = dev->bme280_calib_buf + BME280_TEMPERATURE_SIZE + BME280_PRESS_SIZE; // offset 24: starts at 0xA1

    // Temperature: little-endian pairs, T1 unsigned, T2/T3 signed
    calib_data->dig_T1 = (uint16_t)(((uint16_t)temp_calib_buf[1] << 8) | (uint16_t)temp_calib_buf[0]);
    calib_data->dig_T2 = (int16_t)(uint16_t)(((uint16_t)temp_calib_buf[3] << 8) | (uint16_t)temp_calib_buf[2]);
    calib_data->dig_T3 = (int16_t)(uint16_t)(((uint16_t)temp_calib_buf[5] << 8) | (uint16_t)temp_calib_buf[4]);

    // Pressure: little-endian pairs, P1 unsigned, P2-P9 signed
    calib_data->dig_P1 = (uint16_t)(((uint16_t)pres_calib_buf[1]) << 8  | (uint16_t)pres_calib_buf[0]);
    calib_data->dig_P2 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[3])  << 8 | (uint16_t)pres_calib_buf[2]);
    calib_data->dig_P3 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[5])  << 8 | (uint16_t)pres_calib_buf[4]);
    calib_data->dig_P4 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[7])  << 8 | (uint16_t)pres_calib_buf[6]);
    calib_data->dig_P5 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[9])  << 8 | (uint16_t)pres_calib_buf[8]);
    calib_data->dig_P6 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[11]) << 8 | (uint16_t)pres_calib_buf[10]);
    calib_data->dig_P7 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[13]) << 8 | (uint16_t)pres_calib_buf[12]);
    calib_data->dig_P8 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[15]) << 8 | (uint16_t)pres_calib_buf[14]);
    calib_data->dig_P9 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[17]) << 8 | (uint16_t)pres_calib_buf[16]);

    // Humidity: H4 and H5 share byte [5], requiring nibble unpacking (datasheet Table 16)
    calib_data->dig_H1 = humi_calib_buf[0];                                                                                    // 0xA1
    calib_data->dig_H2 = (int16_t)(uint16_t)(((uint16_t)humi_calib_buf[2] << 8) | (uint16_t)humi_calib_buf[1]);              // 0xE2:0xE1
    calib_data->dig_H3 = humi_calib_buf[3];                                                                                    // 0xE3
    calib_data->dig_H4 = (int16_t)(uint16_t)(((uint16_t)humi_calib_buf[4] << 4) | (uint16_t)(humi_calib_buf[5] & 0x0F));    // 0xE4[7:0] | 0xE5[3:0]
    calib_data->dig_H5 = (int16_t)(uint16_t)(((uint16_t)humi_calib_buf[6] << 4) | (uint16_t)(humi_calib_buf[5] >> 4));      // 0xE6[7:0] | 0xE5[7:4]
    calib_data->dig_H6 = (int8_t)(humi_calib_buf[7]);                                                                         // 0xE7

    return BME280_STATUS_OK;
}

// Fixed-point compensation from datasheet 4.2.3: T in integer degC, H in integer %RH, P in integer Pa
bme280_status_t bme280_read(bme280_t* dev, bme280_int32_t* T, bme280_uint32_t* H, bme280_uint32_t* P) {
    if (!dev || !T || !H || !P) { return BME280_STATUS_NULL_PTR; }

    bme280_calibration_data_t* calib_data = &dev->bme280_calib_data;
    bme280_int32_t adc_T = dev->bme280_raw.bme280_temperature_raw;
    bme280_int32_t adc_P = dev->bme280_raw.bme280_pressure_raw;
    bme280_int32_t adc_H = dev->bme280_raw.bme280_humidity_raw;
    bme280_int64_t var1, var2;

    // Temperature: produces t_fine which is reused by pressure and humidity compensation
    var1 = ((((adc_T >> 3) - ((bme280_int32_t)calib_data->dig_T1 << 1))) *
            ((bme280_int32_t)calib_data->dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((bme280_int32_t)calib_data->dig_T1)) * ((adc_T >> 4) -
            ((bme280_int32_t)calib_data->dig_T1))) >> 12) * ((bme280_int32_t)calib_data->dig_T3)) >> 14;
    bme280_int32_t t_fine = var1 + var2;               // shared fine temperature value
    *T = (bme280_int32_t)((t_fine * 5 + 128) >> 8) / 100;  // result in integer degrees C

    // Pressure: 64-bit integer arithmetic, result in Q24.8 fixed-point Pa (divide by 256 for Pa)
    var1 = ((bme280_int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (bme280_int64_t)calib_data->dig_P6;
    var2 = var2 + ((var1 * (bme280_int64_t)calib_data->dig_P5) << 17);
    var2 = var2 + (((bme280_int64_t)calib_data->dig_P4) << 35);
    var1 = ((var1 * var1 * (bme280_int64_t)calib_data->dig_P3) >> 8) + ((var1 *
            (bme280_int64_t)calib_data->dig_P2) << 12);
    var1 = (((((bme280_int64_t)1) << 47) + var1)) * ((bme280_int64_t)calib_data->dig_P1) >> 33;
    if (var1 == 0) {
        return BME280_STATUS_ERROR;  // dig_P1 was zero; division by zero guard
    }
    bme280_int64_t p_fine = 1048576 - adc_P;
    p_fine = (((p_fine << 31) - var2) * 3125) / var1;
    var1 = (((bme280_int64_t)calib_data->dig_P9) * (p_fine >> 13) * (p_fine >> 13)) >> 25;
    var2 = (((bme280_int64_t)calib_data->dig_P8) * p_fine) >> 19;
    *P = ((p_fine + var1 + var2) >> 8) + (((bme280_int64_t)calib_data->dig_P7) << 4);  // result in integer Pa

    // Humidity: result in Q22.10 fixed-point %RH, clamped to [0, 100] before output
    bme280_int64_t v_x1_u32r;
    v_x1_u32r = (t_fine - ((bme280_int64_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((bme280_int64_t)calib_data->dig_H4) << 20) -
                    (((bme280_int64_t)calib_data->dig_H5) * v_x1_u32r)) +
                    ((bme280_int64_t)16384)) >> 15) * (((((((v_x1_u32r * ((bme280_int64_t)calib_data->dig_H6)) >> 10) *
                    (((v_x1_u32r * ((bme280_int64_t)calib_data->dig_H3)) >> 11) + ((bme280_int64_t)32768))) >> 10) +
                    ((bme280_int64_t)2097152)) * ((bme280_int64_t)calib_data->dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                ((bme280_int64_t)calib_data->dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);              // clamp negative to 0
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);  // clamp to 100 %RH (419430400 = 100 << 22 / 1024 * ... )
    *H = (bme280_uint32_t)(v_x1_u32r >> 12) / 1024;            // result in integer %RH

    return BME280_STATUS_OK;
}

// Floating-point compensation from datasheet appendix 8.1: T in degC, P in Pa, H in %RH (all as double)
bme280_status_t bme280_read_double(bme280_t* dev, double* T, double* H, double* P) {
    if (!dev || !T || !H || !P) { return BME280_STATUS_NULL_PTR; }

    bme280_calibration_data_t* calib_data = &dev->bme280_calib_data;
    bme280_int32_t adc_T = dev->bme280_raw.bme280_temperature_raw;
    bme280_int32_t adc_P = dev->bme280_raw.bme280_pressure_raw;
    bme280_int32_t adc_H = dev->bme280_raw.bme280_humidity_raw;
    double var1, var2, p, var_H;

    // Temperature: produces t_fine shared with pressure and humidity compensation
    var1 = (((double)adc_T) / 16384.0 - ((double)calib_data->dig_T1) / 1024.0) * ((double)calib_data->dig_T2);
    var2 = ((((double)adc_T) / 131072.0 - ((double)calib_data->dig_T1) / 8192.0) * (((double)adc_T) / 131072.0 - ((double)calib_data->dig_T1) / 8192.0)) * ((double)calib_data->dig_T3);
    bme280_int32_t t_fine = (bme280_int32_t)(var1 + var2);  // truncated to int32 for P/H formulas
    *T = (var1 + var2) / 5120.0;                            // result in degrees C

    // Pressure
    var1 = ((double)t_fine / 2.0) - 64000.0;
    var2 = var1 * var1 * ((double)calib_data->dig_P6) / 32768.0;
    var2 = var2 + var1 * ((double)calib_data->dig_P5) * 2.0;
    var2 = (var2 / 4.0) + (((double)calib_data->dig_P4) * 65536.0);
    var1 = (((double)calib_data->dig_P3) * var1 * var1 / 524288.0 + ((double)calib_data->dig_P2) * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * ((double)calib_data->dig_P1);
    if (var1 == 0.0) {
        return BME280_STATUS_ERROR;  // dig_P1 was zero; division by zero guard
    }
    p = 1048576.0 - (double)adc_P;
    p = (p - (var2 / 4096.0)) * 6250.0 / var1;
    var1 = ((double)calib_data->dig_P9) * p * p / 2147483648.0;
    var2 = p * ((double)calib_data->dig_P8) / 32768.0;
    *P = p + (var1 + var2 + ((double)calib_data->dig_P7)) / 16.0;  // result in Pa

    // Humidity: result clamped to [0.0, 100.0] %RH
    var_H = (((double)t_fine) - 76800.0);
    var_H = (adc_H - (((double)calib_data->dig_H4) * 64.0 + ((double)calib_data->dig_H5) / 16384.0 * var_H))
            * (((double)calib_data->dig_H2) / 65536.0 * (1.0 + ((double)calib_data->dig_H6) / 67108864.0 *
                    var_H * (1.0 + ((double)calib_data->dig_H3) / 67108864.0 * var_H)));
    var_H = var_H * (1.0 - ((double)calib_data->dig_H1) * var_H / 524288.0);
    if (var_H > 100.0)
        var_H = 100.0;
    else if (var_H < 0.0)
        var_H = 0.0;
    *H = var_H;  // result in %RH

    return BME280_STATUS_OK;
}

// Preset: forced mode, 1x oversampling all channels, filter off — for once-per-minute weather readings
bme280_status_t bme280_weatherMonitoring(bme280_t* dev, bme280_config_t* cfg) {
    if (!dev || !cfg) { return BME280_STATUS_NULL_PTR; }

    dev->bme280_mode            = FORCED_MODE;
    dev->bme280_pressure_ovs    = BME280_PRESSURE_OVERSAMPLING_X1;
    dev->bme280_temperature_ovs = BME280_TEMPERATURE_OVERSAMPLING_X1;
    dev->bme280_humidity_ovs    = BME280_HUMIDITY_OVERSAMPLING_X1;

    cfg->bme280_filter = BME280_FILTER_OFF;

    return BME280_STATUS_OK;
}

// Preset: forced mode, humidity only (pressure skipped), filter off — lowest power humidity sensing
bme280_status_t bme280_humiditySensing(bme280_t* dev, bme280_config_t* cfg) {
    if (!dev || !cfg) { return BME280_STATUS_NULL_PTR; }

    dev->bme280_mode            = FORCED_MODE;
    dev->bme280_pressure_ovs    = BME280_PRESSURE_OVERSAMPLING_SKIPPED;
    dev->bme280_temperature_ovs = BME280_TEMPERATURE_OVERSAMPLING_X1;
    dev->bme280_humidity_ovs    = BME280_HUMIDITY_OVERSAMPLING_X1;

    cfg->bme280_filter = BME280_FILTER_OFF;

    return BME280_STATUS_OK;
}

// Preset: normal mode, high oversampling, IIR filter x16, 0.5ms standby — optimized for indoor altitude
bme280_status_t bme280_indoorNavigation(bme280_t* dev, bme280_config_t* cfg) {
    if (!dev || !cfg) { return BME280_STATUS_NULL_PTR; }

    dev->bme280_mode            = NORMAL_MODE;
    dev->bme280_pressure_ovs    = BME280_PRESSURE_OVERSAMPLING_X16;
    dev->bme280_temperature_ovs = BME280_TEMPERATURE_OVERSAMPLING_X2;
    dev->bme280_humidity_ovs    = BME280_HUMIDITY_OVERSAMPLING_X1;

    cfg->bme280_standby = BME280_STANDBY_0_5;
    cfg->bme280_filter  = BME280_FILTER_16;

    return BME280_STATUS_OK;
}

// Preset: normal mode, pressure x4, IIR filter x16, 0.5ms standby — fast pressure updates for gaming
bme280_status_t bme280_gaming(bme280_t* dev, bme280_config_t* cfg) {
    if (!dev || !cfg) { return BME280_STATUS_NULL_PTR; }

    dev->bme280_mode            = NORMAL_MODE;
    dev->bme280_pressure_ovs    = BME280_PRESSURE_OVERSAMPLING_X4;
    dev->bme280_temperature_ovs = BME280_TEMPERATURE_OVERSAMPLING_X1;
    dev->bme280_humidity_ovs    = BME280_HUMIDITY_OVERSAMPLING_SKIPPED;

    cfg->bme280_standby = BME280_STANDBY_0_5;
    cfg->bme280_filter  = BME280_FILTER_16;

    return BME280_STATUS_OK;
}
