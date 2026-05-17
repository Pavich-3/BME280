# BME280 Driver

A HAL-independent C driver for the Bosch BME280 environmental sensor (temperature, pressure, humidity).

Written as a practice project to learn embedded driver architecture. Developed and tested on STM32F756ZGTx, but the driver itself has no dependency on any specific MCU or HAL — it works on any platform by providing two function pointers.

---

## Features

- **HAL-independent** — communicates through user-provided `read`/`write` callbacks; works with I2C, SPI, or any other bus
- **Two compensation variants** — fixed-point integer (int32/int64) and floating-point (double), both from the official Bosch datasheet
- **Four preset configurations** — weather monitoring, humidity sensing, indoor navigation, gaming (from datasheet Table 5)
- **Specific error codes** — `COMM_ERROR`, `TIMEOUT`, `DEVICE_NOT_FOUND`, `NULL_PTR` and more, so the caller knows exactly what went wrong
- **Correct register sequencing** — `ctrl_hum` (0xF2) is always written before `ctrl_meas` (0xF4), as required by the datasheet

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

## API

### Setup sequence

```c
// 1. Choose a preset (fills dev mode/oversampling/filter fields)
bme280_indoorNavigation(&dev, &cfg);

// 2. Wire up the bus
bme280_bus_t bus = {0};
bus.dev_addr = BME280_I2C_DEVICE_ADDR_GND << 1;  // 7-bit addr shifted for HAL
bus.ctx      = &hi2c1;                            // passed as void* to read/write
bus.read     = i2c_read;
bus.write    = i2c_write;

// 3. Init, configure, read calibration (once at startup)
bme280_init(&dev, &bus);
bme280_configure(&dev, &cfg);
bme280_read_calibration(&dev);
```

### Measurement loop

```c
// Fixed-point output
bme280_int32_t  T;  // integer degrees C
bme280_uint32_t H;  // integer %RH
bme280_uint32_t P;  // integer Pa

bme280_read_raw(&dev);
bme280_read(&dev, &T, &H, &P);

// Floating-point output
double T, H, P;  // degrees C, %RH, Pa

bme280_read_raw(&dev);
bme280_read_double(&dev, &T, &H, &P);
```

### Writing a platform glue function

```c
bme280_status_t i2c_read(uint8_t dev_addr, uint8_t reg_addr,
                          uint8_t* data, uint8_t len, void* ctx) {
    if (HAL_I2C_Mem_Read((I2C_HandleTypeDef*)ctx, dev_addr, reg_addr,
                          I2C_MEMADD_SIZE_8BIT, data, len, HAL_MAX_DELAY) != HAL_OK) {
        return BME280_STATUS_ERROR;
    }
    return BME280_STATUS_OK;
}
```

The write function follows the same pattern with `HAL_I2C_Mem_Write`.

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
