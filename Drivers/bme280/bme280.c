/*
 * bme280.c
 *
 *  Created on: May 4, 2026
 *      Author: p.baranchuk
 */
#include "bme280.h"
#include "string.h"

bme280_status_t bme280_reset(bme280_t* dev) {
    dev->reset = BME280_RESET;
    dev->bme280_humidity_ovs = 0;
    dev->bme280_pressure_ovs = 0;
    dev->bme280_temperature_ovs = 0;

    return BME280_STATUS_OK;
}

bme280_status_t bme280_read_calibration(bme280_t* dev) {
    memset(dev->_bme280HUMIRegister, 0, sizeof(dev->_bme280HUMIRegister));
    memset(dev->_bme280PRESRegister, 0, sizeof(dev->_bme280PRESRegister));
    memset(dev->_bme280TEMPRegister, 0, sizeof(dev->_bme280TEMPRegister));

    size_t i = 0;
    while(i < BME280_HUMIDITY_SIZE) {
        dev->_bme280HUMIRegister[i] = (BME280_CALIB_HUMI_ADDR_START + i);
        ++i;
    }

    i = 0;
    while(i < BME280_PRESS_SIZE) {
        dev->_bme280PRESRegister[i] = (BME280_CALIB_PRES_ADDR_START + i);
        ++i;
    }

    i = 0;
    while(i < BME280_TEMPERATURE_SIZE) {
        dev->_bme280TEMPRegister[i] = (BME280_CALIB_TEMP_ADDR_START + i);
        ++i;
    }

    return BME280_STATUS_OK;
}

bme280_status_t bme280_read_raw(bme280_t* dev, bme280_raw_t* raw) {
    memset(dev->_bme280HUMIRegister, 0, sizeof(dev->_bme280HUMIRegister));
    memset(dev->_bme280PRESRegister, 0, sizeof(dev->_bme280PRESRegister));
    memset(dev->_bme280TEMPRegister, 0, sizeof(dev->_bme280TEMPRegister));

    size_t i = 0;
    while(i < BME280_HUMIDITY_SIZE) {
        uint8_t addr = (uint8_t)(BME280_HUMI_LSB_ADDR + (uint8_t)i);
        dev->_bme280HUMIRegister[i] = addr;
        if (BME280_HUMI_MSB_ADDR == addr) {
            break;
        }
        ++i;
    }

    i = 0;
    while(i < BME280_PRESS_SIZE) {
        dev->_bme280PRESRegister[i] = (BME280_PRES_XLSB_ADDR + i);
        uint8_t addr = (uint8_t)(BME280_HUMI_LSB_ADDR + (uint8_t)i);
        if (BME280_TEMP_MSB_ADDR == addr) {
            break;
        }
        ++i;
    }

    i = 0;
    while(i < BME280_TEMPERATURE_SIZE) {
        uint8_t addr = (uint8_t)(BME280_HUMI_LSB_ADDR + (uint8_t)i);
        dev->_bme280TEMPRegister[i] = (BME280_TEMP_XLSB_ADDR + i);
        if (BME280_PRES_MSB_ADDR == addr) {
            break;
        }
        ++i;
    }

    return BME280_STATUS_OK;
}

bme280_status_t bme280_convertCalibData(bme280_t* dev) {
    bme280_calibration_data_t* calib_data = &dev->bme280_calib_data;

    uint8_t* humi_calib_buf = dev->bme280_calib_buf.bme280_humi_calib_buf;
    uint8_t* pres_calib_buf = dev->bme280_calib_buf.bme280_pres_calib_buf;
    uint8_t* temp_calib_buf = dev->bme280_calib_buf.bme280_temp_calib_buf;

    calib_data->dig_T1 = (uint16_t)((temp_calib_buf[1] << 8) | temp_calib_buf[0]);
    calib_data->dig_T2 = (int16_t)((temp_calib_buf[3] << 8) | temp_calib_buf[2]);
    calib_data->dig_T3 = (int16_t)((temp_calib_buf[5] << 8) | temp_calib_buf[4]);

    calib_data->dig_P1 = (uint16_t)((pres_calib_buf[1]) << 8 | pres_calib_buf[0]);
    calib_data->dig_P2 = (int16_t)((pres_calib_buf[3]) << 8 | pres_calib_buf[2]);
    calib_data->dig_P3 = (int16_t)((pres_calib_buf[5]) << 8 | pres_calib_buf[4]);
    calib_data->dig_P4 = (int16_t)((pres_calib_buf[7]) << 8 | pres_calib_buf[6]);
    calib_data->dig_P5 = (int16_t)((pres_calib_buf[9]) << 8 | pres_calib_buf[8]);
    calib_data->dig_P6 = (int16_t)((pres_calib_buf[11]) << 8 | pres_calib_buf[10]);
    calib_data->dig_P7 = (int16_t)((pres_calib_buf[13]) << 8 | pres_calib_buf[12]);
    calib_data->dig_P8 = (int16_t)((pres_calib_buf[15]) << 8 | pres_calib_buf[14]);
    calib_data->dig_P9 = (int16_t)((pres_calib_buf[17]) << 8 | pres_calib_buf[16]);

    calib_data->dig_H1 = humi_calib_buf[0];
    calib_data->dig_H2 = (int16_t)((humi_calib_buf[2] << 8) | humi_calib_buf[1]);
    calib_data->dig_H3 = humi_calib_buf[3];
    calib_data->dig_H4 = (int16_t)((humi_calib_buf[5] << 8) | humi_calib_buf[4]);
    calib_data->dig_H5 = (int16_t)((humi_calib_buf[7] << 8) | humi_calib_buf[6]);
    calib_data->dig_H6 = (int8_t)(humi_calib_buf[8]);
}

bme280_status_t bme280_read(bme280_t* dev, float* T, bme280_uint32_t* H, bme280_uint32_t* P) {
    bme280_calibration_data_t* calib_data = &dev->bme280_calib_data;
    bme280_int32_t adc_T = dev->bme280_raw.bme280_temperature_raw;
    bme280_int32_t adc_P = dev->bme280_raw.bme280_pressure_raw;
    bme280_int32_t adc_H = dev->bme280_raw.bme280_humidity_raw;
    bme280_int32_t var1, var2;

    var1 = ((((adc_T >> 3) - ((bme280_int32_t)calib_data->dig_T1 << 1))) *
            ((bme280_int32_t)calib_data->dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((bme280_int32_t)calib_data->dig_T1)) * ((adc_T >> 4) -
            ((bme280_int32_t)calib_data->dig_T1))) >> 12) * ((bme280_int32_t)calib_data->dig_T3)) >> 14;
    bme280_int32_t t_fine = var1 + var2;

    *T = (t_fine * 5 + 128) >> 8;

    var1 = ((bme280_int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (bme280_int64_t)calib_data->dig_P6;
    var2 = var2 + ((var1 * (bme280_int64_t)calib_data->dig_P5) << 17);
    var2 = var2 + (((bme280_int64_t)calib_data->dig_P4) << 35);
    var1 = ((var1 * var1 * (bme280_int64_t)calib_data->dig_P3) >> 8) + ((var1 *
            (bme280_int64_t)calib_data->dig_P2) << 12);
    var1 = (((((bme280_int64_t)1) << 47) + var1)) * ((bme280_int64_t)calib_data->dig_P1) >> 33;
    if(var1 == 0) {
        return 0; // avoid exception caused by division by zero
    }

    *P = 1048576 - adc_P;
    *P = (((*P << 31) - var2) * 3125) / var1;
    var1 = (((bme280_int64_t)calib_data->dig_P9) * (*P >> 13) * (*P >> 13)) >> 25;
    var2 =(((bme280_int64_t)calib_data->dig_P8) * *P) >> 19;
    *P = ((*P + var1 + var2) >> 8) + (((bme280_int64_t)calib_data->dig_P7) << 4);

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
