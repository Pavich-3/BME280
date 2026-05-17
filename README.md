# BME280 Driver

A HAL-independent C driver for the Bosch BME280 environmental sensor (temperature, pressure, humidity).

Written as a practice project to learn embedded driver architecture. Developed and tested on STM32F756ZGTx, but the driver itself has no dependency on any specific MCU or HAL — it works on any platform by providing two function pointers.

---

## What is the BME280

The BME280 is a small digital sensor from Bosch that measures three things:

- **Temperature** — range -40 to +85 °C, resolution 0.01 °C
- **Humidity** — range 0 to 100 %RH, resolution ~0.008 %RH
- **Pressure** — range 300 to 1100 hPa (equivalent to altitudes of -500 to +9000 m), resolution 0.18 Pa

It communicates over I2C or SPI. Internally, the sensor stores factory-calibrated compensation coefficients in its own memory. Raw ADC values from the sensor are meaningless on their own — they must be passed through compensation formulas together with those coefficients to produce real-world values. This driver handles all of that.

---

## Features

- **HAL-independent** — communicates through user-provided `read`/`write` callbacks; works with I2C, SPI, or any other bus
- **Two compensation variants** — fixed-point integer (int32/int64) and floating-point (double), both from the official Bosch datasheet
- **Four preset configurations** — weather monitoring, humidity sensing, indoor navigation, gaming (from datasheet Table 5)
- **Specific error codes** — `COMM_ERROR`, `TIMEOUT`, `DEVICE_NOT_FOUND`, `NULL_PTR` and more, so the caller knows exactly what went wrong
- **Correct register sequencing** — `ctrl_hum` (0xF2) is always written before `ctrl_meas` (0xF4), as required by the datasheet

---

## Hardware

### Wiring (I2C)

| BME280 pin | Connect to |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| SDA | SDA (with 4.7 kΩ pull-up to 3.3 V) |
| SCL | SCL (with 4.7 kΩ pull-up to 3.3 V) |
| SDO | GND or VDDIO |

### I2C address

The SDO pin selects the 7-bit I2C address:

| SDO | 7-bit address | 8-bit address (HAL) |
|---|---|---|
| GND | 0x76 | 0xEC |
| VDDIO | 0x77 | 0xEE |

HAL functions (`HAL_I2C_Mem_Read`, `HAL_I2C_Mem_Write`) expect the address left-shifted by 1. The driver header provides `BME280_I2C_DEVICE_ADDR_GND` and `BME280_I2C_DEVICE_ADDR_VDDIO` as 7-bit values — shift them when filling `bme280_bus_t`:

```c
bus.dev_addr = BME280_I2C_DEVICE_ADDR_GND << 1;  // 0x76 -> 0xEC
```

---

## Project Structure

```
Drivers/bme280/
├── bme280_regs.h   — register addresses and bit positions
├── bme280.h        — types, structs, enums, function prototypes
└── bme280.c        — driver implementation

Core/Src/
└── main.c          — STM32 HAL glue (i2c_read / i2c_write) and application loop
```

---

## Architecture

The driver uses a **bus abstraction struct** (`bme280_bus_t`) that holds two function pointers — one for read, one for write — along with the device address and a `void* ctx` passed to every call. This means the driver never calls any HAL or platform function directly.

```
Application (main.c)
    │
    ├── fills bme280_bus_t with HAL wrappers
    ├── calls bme280_init / bme280_configure / bme280_read_calibration
    └── calls bme280_read_raw + bme280_read in a loop
          │
          ▼
Driver (bme280.c)
    │
    └── calls dev->bme280_bus.read / .write
          │
          ▼
Platform glue (main.c: i2c_read / i2c_write)
    │
    └── calls HAL_I2C_Mem_Read / HAL_I2C_Mem_Write
```

To port to a different MCU, only the platform glue functions need to change. The driver files are untouched.

---

## Quick Start

This is a complete example from zero to first reading on STM32 with HAL.

**1. Write the platform glue functions**

```c
bme280_status_t i2c_read(uint8_t dev_addr, uint8_t reg_addr,
                          uint8_t* data, uint8_t len, void* ctx) {
    if (HAL_I2C_Mem_Read((I2C_HandleTypeDef*)ctx, dev_addr, reg_addr,
                          I2C_MEMADD_SIZE_8BIT, data, len, HAL_MAX_DELAY) != HAL_OK) {
        return BME280_STATUS_COMM_ERROR;
    }
    return BME280_STATUS_OK;
}

bme280_status_t i2c_write(uint8_t dev_addr, uint8_t reg_addr,
                           uint8_t* data, uint8_t len, void* ctx) {
    if (HAL_I2C_Mem_Write((I2C_HandleTypeDef*)ctx, dev_addr, reg_addr,
                           I2C_MEMADD_SIZE_8BIT, data, len, HAL_MAX_DELAY) != HAL_OK) {
        return BME280_STATUS_COMM_ERROR;
    }
    return BME280_STATUS_OK;
}
```

**2. Initialize at startup**

```c
bme280_t        dev = {0};
bme280_config_t cfg = {0};

// Choose a preset — fills dev.mode, dev.oversampling fields and cfg.filter
bme280_indoorNavigation(&dev, &cfg);

// Wire up the bus
bme280_bus_t bus = {0};
bus.dev_addr = BME280_I2C_DEVICE_ADDR_GND << 1;  // SDO pin to GND
bus.ctx      = &hi2c1;
bus.read     = i2c_read;
bus.write    = i2c_write;

// Init sequence — order matters
bme280_init(&dev, &bus);            // validates chip ID
bme280_configure(&dev, &cfg);       // writes registers 0xF5, 0xF2, 0xF4
bme280_read_calibration(&dev);      // reads factory coefficients — call once only
```

**3. Read in a loop**

```c
bme280_int32_t  temperature;  // e.g. 24  -> 24 °C
bme280_uint32_t humidity;     // e.g. 55  -> 55 %RH
bme280_uint32_t pressure;     // e.g. 101325 -> 101325 Pa (1013.25 hPa)

bme280_read_raw(&dev);
bme280_read(&dev, &temperature, &humidity, &pressure);
```

---

## API

### Setup

```c
bme280_status_t bme280_init(bme280_t* dev, const bme280_bus_t* bus);
```
Copies the bus into the device struct, reads the chip ID register and verifies it is `0x60`. Must be called before any other function.

```c
bme280_status_t bme280_configure(bme280_t* dev, const bme280_config_t* cfg);
```
Writes the three control registers in the correct order. Puts the device into sleep first, then writes config (0xF5), ctrl_hum (0xF2), ctrl_meas (0xF4).

```c
bme280_status_t bme280_read_calibration(bme280_t* dev);
```
Reads factory calibration coefficients from the sensor and parses them into the device struct. **Call once at startup, never in the measurement loop.** The coefficients never change.

```c
bme280_status_t bme280_reset(bme280_t* dev);
```
Sends a soft reset command (`0xB6`) to the sensor and clears the device struct. After reset, the full init sequence must be repeated.

```c
bme280_status_t bme280_set_mode(bme280_t* dev, bme280_mode_t mode);
```
Changes the operating mode without touching oversampling settings. Uses read-modify-write on register 0xF4.

### Data acquisition

```c
bme280_status_t bme280_read_raw(bme280_t* dev);
```
In **forced mode**: triggers a single measurement and polls the status register until it completes, then reads all 8 data bytes in one burst. In **normal mode**: reads the latest result directly without triggering.

```c
bme280_status_t bme280_read(bme280_t* dev, bme280_int32_t* T,
                             bme280_uint32_t* H, bme280_uint32_t* P);
```
Applies fixed-point integer compensation formulas. Output: T in °C, H in %RH, P in Pa — all as integers. Use this when floating-point is unavailable or performance matters.

```c
bme280_status_t bme280_read_double(bme280_t* dev, double* T,
                                    double* H, double* P);
```
Applies floating-point compensation formulas. Output: T in °C, H in %RH, P in Pa — all as `double`. Easier to work with but requires FPU support. Use this when precision and readability matter more than performance.

---

## Compensation Variants

The BME280 datasheet provides two sets of formulas:

| | `bme280_read` | `bme280_read_double` |
|---|---|---|
| Arithmetic | Integer (int32/int64) | Floating-point (double) |
| FPU needed | No | Yes (recommended) |
| Speed | Faster | Slower without FPU |
| Output | Integer °C, %RH, Pa | Double °C, %RH, Pa |
| When to use | Resource-constrained MCUs, no FPU | STM32F4/F7/H7 with FPU, when precision or readability matters |

Both variants call `bme280_read_raw` first to populate the raw ADC values, then apply the formulas.

---

## Preset Configurations

All presets are taken from Bosch datasheet Table 5.

| Preset | Mode | T oversampling | P oversampling | H oversampling | Filter |
|---|---|---|---|---|---|
| `bme280_weatherMonitoring` | Forced | ×1 | ×1 | ×1 | Off |
| `bme280_humiditySensing` | Forced | ×1 | Skipped | ×1 | Off |
| `bme280_indoorNavigation` | Normal | ×2 | ×16 | ×1 | ×16 |
| `bme280_gaming` | Normal | ×1 | ×4 | Skipped | ×16 |

Each preset fills `bme280_t` (mode, oversampling) and `bme280_config_t` (filter, standby). Call `bme280_configure` afterwards to apply settings to the hardware.

**Forced mode** — sensor takes one measurement then returns to sleep. The application must trigger each measurement explicitly. Best for low-power applications with infrequent readings.

**Normal mode** — sensor continuously measures at the configured standby interval. The application reads the latest result whenever it needs it. Best for continuous monitoring.

---

## Status Codes

| Code | Meaning |
|---|---|
| `BME280_STATUS_OK` | Success |
| `BME280_STATUS_ERROR` | Generic unspecified error |
| `BME280_STATUS_RESET` | Device was reset |
| `BME280_STATUS_NULL_PTR` | A required pointer argument was NULL |
| `BME280_STATUS_INVALID_ARG` | Argument was non-null but had an invalid value |
| `BME280_STATUS_INVALID_STATE` | Device not initialized or bus not configured |
| `BME280_STATUS_TIMEOUT` | Forced mode measurement did not complete in time |
| `BME280_STATUS_COMM_ERROR` | I2C/SPI read or write returned a failure |
| `BME280_STATUS_DEVICE_NOT_FOUND` | Chip ID was read but did not match 0x60 |

---

## Notes

- **`bme280_read_calibration` must be called once at startup**, not in the measurement loop. Calibration data is factory-programmed and never changes.
- **Forced mode** requires triggering each measurement manually with `bme280_set_mode(FORCED_MODE)`. `bme280_read_raw` handles this automatically when `dev.bme280_mode == FORCED_MODE`.
- **Normal mode** continuously measures in the background at the configured standby interval. No trigger is needed — just call `bme280_read_raw` to read the latest result.
- **D-Cache (STM32F7/H7):** If using DMA-based I2C, configure an MPU non-cacheable region for the receive buffer. Blocking I2C (used here) is not affected.
- The driver is written in C99.

---

## Author

Pavlo Baranchuk
