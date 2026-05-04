/*
 * bme280.c
 *
 *  Created on: May 4, 2026
 *      Author: p.baranchuk
 */
#include "bme280.h"

bme280_status_t bme280_reset(bme280_t* dev) {
    dev->reset = BME280_RESET;
    dev->bme280_humidity_ovs = 0;
    dev->bme280_pressure_ovs = 0;
    dev->bme280_temperature_ovs = 0;

    return BME280_STATUS_OK;
}

bme280_status_t bme280_read_calibration(bme280_t* dev) {

}
