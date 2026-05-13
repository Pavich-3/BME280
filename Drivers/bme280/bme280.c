/*
 * bme280.c
 *
 *  Created on: May 4, 2026
 *      Author: p.baranchuk
 */
#include "bme280.h"
#include "string.h"

bme280_status_t bme280_init(bme280_t* dev, const bme280_bus_t* bus) {
    if (!dev || !bus) {
        return BME280_STATUS_INVALID_ARG;
    }
    if (!bus->read || !bus->write) {
        return BME280_STATUS_INVALID_ARG;
    }

    uint8_t currentID = 0;
    dev->status = BME280_STATUS_OK;
    dev->bme280_bus = *bus;

    if (bus->read(bus->dev_addr, BME280_ID_ADDR, &currentID, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_ERROR;
    }
    if (currentID != BME280_ID) {
        return BME280_STATUS_INVALID_ARG;
    }

    if (bus->write(bus->dev_addr, BME280_CTRL_MEAS_ADDR, (uint8_t)((uint8_t)dev->bme280_temperature_ovs | (uint8_t)dev->bme280_pressure_ovs | (uint8_t)dev->bme280_mode), 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_ERROR;
    }
    if (bus->write(bus->dev_addr, BME280_CTRL_HUM_ADDR, (uint8_t)dev->bme280_humidity_ovs, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_ERROR;
    }

    return BME280_STATUS_OK;
}

bme280_status_t bme280_reset(bme280_t* dev) {
    if (!dev || !dev->bme280_bus.write) {
        return BME280_STATUS_INVALID_ARG;
    }

    bme280_bus_t bme280_bus = dev->bme280_bus;
    memset(dev, 0, sizeof(*dev));
    dev->reset = BME280_RESET;
    dev->bme280_bus = bme280_bus;

    if (dev->bme280_bus.write(dev->bme280_bus.dev_addr, BME280_RESET_ADDR, &BME280_RESET, 1, dev->bme280_bus.ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_ERROR;
    }

    return BME280_STATUS_OK;
}

bme280_status_t bme280_set_mode(bme280_t* dev, bme280_mode_t mode) {
    if (!dev || !dev->bme280_bus.read || !dev->bme280_bus.write) {
        return BME280_STATUS_INVALID_ARG;
    }

    bme280_bus_t* bus = &dev->bme280_bus;

    uint8_t ctrlMeas = 0;
    if (bus->read(bus->dev_addr, BME280_CTRL_MEAS_ADDR, &ctrlMeas, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_ERROR;
    }
    ctrlMeas = ((ctrlMeas & 0xFC) | (mode & 0x03));
    if (bus->write(bus->dev_addr, BME280_CTRL_MEAS_ADDR, ctrlMeas, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_ERROR;
    }

    return BME280_STATUS_OK;
}

bme280_status_t bme280_read_calibration(bme280_t* dev) {
    if (!dev || !dev->bme280_bus.read || !dev->bme280_bus.write) {
        return BME280_STATUS_INVALID_ARG;
    }

    uint8_t len = BME280_CALIB_PRES_ADDR_END - BME280_CALIB_TEMP_ADDR_START + 1;
    if (dev->bme280_bus.read(dev->bme280_bus.dev_addr, BME280_CALIB_TEMP_ADDR_START, dev->bme280_calib_buf, len, dev->bme280_bus.ctx) != BME280_STATUS_OK) {
        dev->status = BME280_STATUS_ERROR;
        return BME280_STATUS_ERROR;
    }


    if (dev->bme280_bus.read(dev->bme280_bus.dev_addr, BME280_CALIB_HUMI_ADDR_START, (dev->bme280_calib_buf + len), BME280_CALIB_HUMI_SIZE_START, dev->bme280_bus.ctx) != BME280_STATUS_OK) {
        dev->status = BME280_STATUS_ERROR;
        return BME280_STATUS_ERROR;
    }
    len += BME280_CALIB_HUMI_SIZE_START;
    if (dev->bme280_bus.read(dev->bme280_bus.dev_addr, BME280_CALIB_HUMI_ADDR_CONTINUE, (dev->bme280_calib_buf + len), (BME280_CALIB_HUMI_ADDR_END - BME280_CALIB_HUMI_ADDR_CONTINUE + 1), dev->bme280_bus.ctx) != BME280_STATUS_OK) {
        dev->status = BME280_STATUS_ERROR;
        return BME280_STATUS_ERROR;
    }

    if (bme280_convertCalibData(dev) != BME280_STATUS_OK) {
        dev->status = BME280_STATUS_ERROR;
        return BME280_STATUS_ERROR;
    }

    return BME280_STATUS_OK;
}

bme280_status_t bme280_set_config(bme280_t* dev, bme280_config_t* cfg) {
    bme280_bus_t* bus = &dev->bme280_bus;
    if (!dev || !bus->read || !bus->write) {
        return BME280_STATUS_INVALID_ARG;
    }

    uint8_t ctrlMeas = 0;
    if (bus->read(bus->dev_addr, BME280_CTRL_MEAS_ADDR, &ctrlMeas, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_ERROR;
    }
    ctrlMeas = ((ctrlMeas & 0xFC) | (SLEEP_MODE & 0x03));
    if (bus->write(bus->dev_addr, BME280_CTRL_MEAS_ADDR, ctrlMeas, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_ERROR;
    }

    if (bus->write(bus->dev_addr, BME280_CONFIG_ADDR, (cfg->bme280_standby | cfg->bme280_filter | cfg->bme280_spi_status), 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_ERROR;
    }

    ctrlMeas = ((ctrlMeas & 0xFC) | (dev->bme280_mode & 0x03));
    if (bus->write(bus->dev_addr, BME280_CTRL_MEAS_ADDR, ctrlMeas, 1, bus->ctx) != BME280_STATUS_OK) {
        return BME280_STATUS_ERROR;
    }

    return BME280_STATUS_OK;
}

bme280_status_t bme280_read_raw(bme280_t* dev) {
    if (!dev || !dev->bme280_bus.read || !dev->bme280_bus.write) {
        return BME280_STATUS_INVALID_ARG;
    }

    if (dev->bme280_mode == FORCED_MODE) {
        uint8_t status = 0;
        do {
            if (dev->bme280_bus.read(dev->bme280_bus.dev_addr, BME280_STATUS_ADDR, &status, 1, dev->bme280_bus.ctx) != BME280_STATUS_OK) {
                dev->status = BME280_STATUS_ERROR;
                return BME280_STATUS_ERROR;
            }

        } while (status & (1u << BME280_STATUS_MEASURING));
    }

    const uint8_t len = BME280_HUMI_LSB_ADDR - BME280_PRES_MSB_ADDR + 1;
    uint8_t bme280_data_raw[len];
    if (dev->bme280_bus.read(dev->bme280_bus.dev_addr, BME280_PRES_MSB_ADDR, bme280_data_raw, len, dev->bme280_bus.ctx) != BME280_STATUS_OK) {
        dev->status = BME280_STATUS_ERROR;
        return BME280_STATUS_ERROR;
    }

    dev->bme280_raw.bme280_pressure_raw = (bme280_uint32_t)(((bme280_uint16_t)bme280_data_raw[0] << 12) | ((bme280_uint16_t)bme280_data_raw[1] << 4) | ((bme280_uint16_t)bme280_data_raw[2] >> 4));
    dev->bme280_raw.bme280_temperature_raw = (bme280_uint32_t)(((bme280_uint16_t)bme280_data_raw[3] << 12) | ((bme280_uint16_t)bme280_data_raw[4] << 4) | ((bme280_uint16_t)bme280_data_raw[5] >> 4));
    dev->bme280_raw.bme280_humidity_raw = (bme280_uint16_t)(((bme280_uint16_t)bme280_data_raw[6] << 8) | ((bme280_uint16_t)bme280_data_raw[7]));

    return BME280_STATUS_OK;
}

bme280_status_t bme280_convertCalibData(bme280_t* dev) {
    bme280_calibration_data_t* calib_data = &dev->bme280_calib_data;

    uint8_t* temp_calib_buf = dev->bme280_calib_buf;
    uint8_t* pres_calib_buf = dev->bme280_calib_buf + BME280_TEMPERATURE_SIZE;
    uint8_t* humi_calib_buf = dev->bme280_calib_buf + BME280_TEMPERATURE_SIZE + BME280_PRESS_SIZE;

    calib_data->dig_T1 = (uint16_t)(((uint16_t)temp_calib_buf[1] << 8) | (uint16_t)temp_calib_buf[0]);
    calib_data->dig_T2 = (int16_t)(uint16_t)(((uint16_t)temp_calib_buf[3] << 8) | (uint16_t)temp_calib_buf[2]);
    calib_data->dig_T3 = (int16_t)(uint16_t)(((uint16_t)temp_calib_buf[5] << 8) | (uint16_t)temp_calib_buf[4]);

    calib_data->dig_P1 = (uint16_t)(((uint16_t)pres_calib_buf[1]) << 8 | (uint16_t)pres_calib_buf[0]);
    calib_data->dig_P2 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[3]) << 8 | (uint16_t)pres_calib_buf[2]);
    calib_data->dig_P3 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[5]) << 8 | (uint16_t)pres_calib_buf[4]);
    calib_data->dig_P4 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[7]) << 8 | (uint16_t)pres_calib_buf[6]);
    calib_data->dig_P5 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[9]) << 8 | (uint16_t)pres_calib_buf[8]);
    calib_data->dig_P6 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[11]) << 8 | (uint16_t)pres_calib_buf[10]);
    calib_data->dig_P7 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[13]) << 8 | (uint16_t)pres_calib_buf[12]);
    calib_data->dig_P8 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[15]) << 8 | (uint16_t)pres_calib_buf[14]);
    calib_data->dig_P9 = (int16_t)(uint16_t)(((uint16_t)pres_calib_buf[17]) << 8 | (uint16_t)pres_calib_buf[16]);

    calib_data->dig_H1 = humi_calib_buf[0];
    calib_data->dig_H2 = (int16_t)(uint16_t)(((uint16_t)humi_calib_buf[2] << 8) | (uint16_t)humi_calib_buf[1]);
    calib_data->dig_H3 = humi_calib_buf[3];
    calib_data->dig_H4 = (int16_t)(uint16_t)(((uint16_t)humi_calib_buf[4] << 4) | (uint16_t)(humi_calib_buf[5] & 0x0F));
    calib_data->dig_H5 = (int16_t)(uint16_t)(((uint16_t)humi_calib_buf[6] << 4) | (uint16_t)(humi_calib_buf[5] >> 4));
    calib_data->dig_H6 = (int8_t)(humi_calib_buf[7]);

    return BME280_STATUS_OK;
}

bme280_status_t bme280_read(bme280_t* dev, bme280_uint32_t* T, bme280_uint32_t* H, bme280_uint32_t* P) {
    bme280_calibration_data_t* calib_data = &dev->bme280_calib_data;
    bme280_int32_t adc_T = dev->bme280_raw.bme280_temperature_raw;
    bme280_int32_t adc_P = dev->bme280_raw.bme280_pressure_raw;
    bme280_int32_t adc_H = dev->bme280_raw.bme280_humidity_raw;
    bme280_int64_t var1, var2;

    var1 = ((((adc_T >> 3) - ((bme280_int32_t)calib_data->dig_T1 << 1))) *
            ((bme280_int32_t)calib_data->dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((bme280_int32_t)calib_data->dig_T1)) * ((adc_T >> 4) -
            ((bme280_int32_t)calib_data->dig_T1))) >> 12) * ((bme280_int32_t)calib_data->dig_T3)) >> 14;
    bme280_int32_t t_fine = var1 + var2;

    *T = (bme280_uint32_t)((t_fine * 5 + 128) >> 8) / 100.0f;

    var1 = ((bme280_int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (bme280_int64_t)calib_data->dig_P6;
    var2 = var2 + ((var1 * (bme280_int64_t)calib_data->dig_P5) << 17);
    var2 = var2 + (((bme280_int64_t)calib_data->dig_P4) << 35);
    var1 = ((var1 * var1 * (bme280_int64_t)calib_data->dig_P3) >> 8) + ((var1 *
            (bme280_int64_t)calib_data->dig_P2) << 12);
    var1 = (((((bme280_int64_t)1) << 47) + var1)) * ((bme280_int64_t)calib_data->dig_P1) >> 33;
    if (var1 == 0) {
        return BME280_STATUS_ERROR; // avoid exception caused by division by zero
    }

    bme280_int64_t p_fine = 1048576 - adc_P;
    p_fine = (((p_fine << 31) - var2) * 3125) / var1;
    var1 = (((bme280_int64_t)calib_data->dig_P9) * (p_fine >> 13) * (p_fine >> 13)) >> 25;
    var2 =(((bme280_int64_t)calib_data->dig_P8) * p_fine) >> 19;
    *P = ((p_fine + var1 + var2) >> 8) + (((bme280_int64_t)calib_data->dig_P7) << 4);

    bme280_int64_t v_x1_u32r;
    v_x1_u32r = (t_fine - ((bme280_int64_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((bme280_int64_t)calib_data->dig_H4) << 20) -
                    (((bme280_int64_t)calib_data->dig_H5) * v_x1_u32r)) +
                    ((bme280_int64_t)16384)) >> 15) * (((((((v_x1_u32r * ((bme280_int64_t)calib_data->dig_H6)) >> 10) *
                    (((v_x1_u32r * ((bme280_int64_t)calib_data->dig_H3)) >> 11) + ((bme280_int64_t)32768))) >> 10) +
                    ((bme280_int64_t)2097152)) * ((bme280_int64_t)calib_data->dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                ((bme280_int64_t)calib_data->dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400? 419430400: v_x1_u32r);
    *H = (bme280_uint32_t)(v_x1_u32r>>12);

    return BME280_STATUS_OK;
}

bme280_status_t bme280_weatherMonitoring(bme280_t* dev, bme280_config_t* cfg) {
    dev->bme280_mode = FORCED_MODE;
    dev->bme280_pressure_ovs = BME280_PRESSURE_OVERSAMPLING_X1;
    dev->bme280_temperature_ovs = BME280_TEMPERATURE_OVERSAMPLING_X1;
    dev->bme280_humidity_ovs = BME280_HUMIDITY_OVERSAMPLING_X1;

    cfg->bme280_filter = BME280_FILTER_OFF;

    return BME280_STATUS_OK;
}

bme280_status_t bme280_humiditySensing(bme280_t* dev, bme280_config_t* cfg) {
    dev->bme280_mode = FORCED_MODE;
    dev->bme280_pressure_ovs = BME280_PRESSURE_OVERSAMPLING_SKIPPED;
    dev->bme280_temperature_ovs = BME280_TEMPERATURE_OVERSAMPLING_X1;
    dev->bme280_humidity_ovs = BME280_HUMIDITY_OVERSAMPLING_X1;

    cfg->bme280_filter = BME280_FILTER_OFF;

    return BME280_STATUS_OK;
}

bme280_status_t bme280_indoorNavigation(bme280_t* dev, bme280_config_t* cfg) {
    dev->bme280_mode = NORMAL_MODE;
    dev->bme280_pressure_ovs = BME280_PRESSURE_OVERSAMPLING_X16;
    dev->bme280_temperature_ovs = BME280_TEMPERATURE_OVERSAMPLING_X2;
    dev->bme280_humidity_ovs = BME280_HUMIDITY_OVERSAMPLING_X1;

    cfg->bme280_standby = BME280_STANDBY_0_5;
    cfg->bme280_filter = BME280_FILTER_16;

    return BME280_STATUS_OK;
}

bme280_status_t bme280_gaming(bme280_t* dev, bme280_config_t* cfg) {
    dev->bme280_mode = NORMAL_MODE;
    dev->bme280_pressure_ovs = BME280_PRESSURE_OVERSAMPLING_X4;
    dev->bme280_temperature_ovs = BME280_TEMPERATURE_OVERSAMPLING_X1;
    dev->bme280_humidity_ovs = BME280_HUMIDITY_OVERSAMPLING_SKIPPED;

    cfg->bme280_standby = BME280_STANDBY_0_5;
    cfg->bme280_filter = BME280_FILTER_16;

    return BME280_STATUS_OK;
}
