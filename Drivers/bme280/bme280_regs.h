/*
 * bme280_regs.h
 *
 *  Created on: May 4, 2026
 *      Author: p.baranchuk
 */

#ifndef BME280_BME280_REGS_H_
#define BME280_BME280_REGS_H_

#define BME280_I2C_DEVICE_ADDR_GND   0b1110110  // SDO pin pulled to GND  -> 0x76
#define BME280_I2C_DEVICE_ADDR_VDDIO 0b1110111  // SDO pin pulled to VDDIO -> 0x77

#define BME280_ID_ADDR     0xD0  // read-only chip ID register, expected value 0x60
#define BME280_RESET_ADDR  0xE0  // write 0xB6 to trigger soft reset
#define BME280_STATUS_ADDR 0xF3  // bit 3: measuring, bit 0: im_update

#define BME280_CONFIG_ADDR    0xF5  // standby time, IIR filter, SPI mode
#define BME280_CTRL_HUM_ADDR  0xF2  // humidity oversampling; changes take effect only after ctrl_meas is written
#define BME280_CTRL_MEAS_ADDR 0xF4  // temperature/pressure oversampling and mode bits

#define BME280_CONFIG_STANDBY    5  // bit position of t_sb[2:0] in 0xF5
#define BME280_CONFIG_FILTER     2  // bit position of filter[2:0] in 0xF5
#define BME280_CONFIG_SPI_STATUS 0  // bit position of spi3w_en in 0xF5

#define BME280_STATUS_MEASURING 3  // bit position: 1 = measurement in progress
#define BME280_STATUS_IM_UPDATE 0  // bit position: 1 = NVM data being copied
#define BME280_MODE    0  // bit position of mode[1:0] in 0xF4
#define BME280_HUMI_OVS 0  // bit position of osrs_h[2:0] in 0xF2
#define BME280_TEMP_OVS 5  // bit position of osrs_t[2:0] in 0xF4
#define BME280_PRES_OVS 2  // bit position of osrs_p[2:0] in 0xF4

// Calibration data: temperature and pressure (continuous burst 0x88-0x9F)
#define BME280_CALIB_TEMP_ADDR_START (0x88)
#define BME280_CALIB_TEMP_ADDR_END   (0x8Du)
#define BME280_CALIB_PRES_ADDR_START (0x8Eu)
#define BME280_CALIB_PRES_ADDR_END   (0x9Fu)

// Calibration data: humidity (split into two separate reads)
#define BME280_CALIB_HUMI_ADDR_START    (0xA1u)  // dig_H1 only (1 byte)
#define BME280_CALIB_HUMI_ADDR_CONTINUE (0xE1u)  // dig_H2 through dig_H6
#define BME280_CALIB_HUMI_ADDR_END      (0xE7u)

// Raw measurement data registers (single burst 0xF7-0xFE, 8 bytes)
#define BME280_PRES_MSB_ADDR  (0xF7u)  // pressure[19:12]
#define BME280_PRES_XLSB_ADDR (0xF9u)  // pressure[3:0] in bits [7:4]
#define BME280_TEMP_MSB_ADDR  (0xFAu)  // temperature[19:12]
#define BME280_TEMP_XLSB_ADDR (0xFCu)  // temperature[3:0] in bits [7:4]
#define BME280_HUMI_MSB_ADDR  (0xFDu)  // humidity[15:8]
#define BME280_HUMI_LSB_ADDR  (0xFEu)  // humidity[7:0]

#endif /* BME280_BME280_REGS_H_ */
