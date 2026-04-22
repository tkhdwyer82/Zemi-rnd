/**
 * @file mlx90393.h
 * @brief MLX90393 3D Magnetic Sensor Driver — nRF52 (nRF5 SDK / nrfx)
 *
 * Zemi R&D 2026 — nRF52 Wearable Firmware Scaffold
 *
 * Contractor: Timothy Dwyer (TKD Research and Consulting)
 * Company:    Zemi Pty Ltd
 * Created:    April 2026
 *
 * Target MCU: nRF52840 (Zemi wearable)
 * Interface:  I2C via nrfx_twim (400 kHz Fast Mode)
 * Sensor:     Melexis MLX90393 — 3-axis magnetic field sensor
 *
 * Usage:
 *   mlx90393_config_t cfg = MLX90393_DEFAULT_CONFIG();
 *   mlx90393_dev_t    dev;
 *   mlx90393_init(&dev, &cfg);
 *   mlx90393_start_burst(&dev, MLX90393_AXIS_XYZ);
 *   // in loop:
 *   mlx90393_measurement_t m;
 *   mlx90393_read_burst(&dev, &m);
 *   float mag = mlx90393_magnitude(&m);
 */

#ifndef MLX90393_H
#define MLX90393_H

#include <stdint.h>
#include <stdbool.h>
#include "nrfx_twim.h"
#include "sdk_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── I2C Configuration ────────────────────────────────────────────────
#define MLX90393_I2C_ADDR_BASE      0x0C    /**< A0=0, A1=0 */
#define MLX90393_I2C_ADDR_A         0x0C
#define MLX90393_I2C_ADDR_B         0x0D
#define MLX90393_I2C_ADDR_C         0x0E
#define MLX90393_I2C_ADDR_D         0x0F

// ── MLX90393 Command Set ────────────────────────────────────────────
#define MLX90393_CMD_SB             0x10    /**< Start Burst mode */
#define MLX90393_CMD_SW             0x20    /**< Start Wake-on-change */
#define MLX90393_CMD_SM             0x30    /**< Start Single Measurement */
#define MLX90393_CMD_RM             0x40    /**< Read Measurement */
#define MLX90393_CMD_RR             0x50    /**< Read Register */
#define MLX90393_CMD_WR             0x60    /**< Write Register */
#define MLX90393_CMD_EX             0x80    /**< Exit mode */
#define MLX90393_CMD_HR             0xD0    /**< Memory Recall */
#define MLX90393_CMD_HS             0x70    /**< Memory Store */
#define MLX90393_CMD_RT             0xF0    /**< Reset */

// ── Axis Selection Bitmask ──────────────────────────────────────────
#define MLX90393_AXIS_X             0x02
#define MLX90393_AXIS_Y             0x04
#define MLX90393_AXIS_Z             0x08
#define MLX90393_AXIS_T             0x01    /**< Temperature */
#define MLX90393_AXIS_XYZ           (MLX90393_AXIS_X | MLX90393_AXIS_Y | MLX90393_AXIS_Z)
#define MLX90393_AXIS_XYZT          (MLX90393_AXIS_XYZ | MLX90393_AXIS_T)

// ── Register Addresses ──────────────────────────────────────────────
#define MLX90393_REG_0              0x00    /**< GAIN_SEL, HALLCONF */
#define MLX90393_REG_1              0x01    /**< BURST_DATARATE, BURST_SEL, TCMP_EN */
#define MLX90393_REG_2              0x02    /**< OSR, DIG_FILT, RES_XYZ */
#define MLX90393_REG_3              0x03    /**< SENS_TC_HT, OFFSET_X */

// ── Gain Settings ───────────────────────────────────────────────────
typedef enum {
    MLX90393_GAIN_5X    = 0x00,
    MLX90393_GAIN_4X    = 0x01,
    MLX90393_GAIN_3X    = 0x02,
    MLX90393_GAIN_2_5X  = 0x03,
    MLX90393_GAIN_2X    = 0x04,
    MLX90393_GAIN_1_67X = 0x05,
    MLX90393_GAIN_1_33X = 0x06,
    MLX90393_GAIN_1X    = 0x07,
} mlx90393_gain_t;

// ── Oversampling Ratio ──────────────────────────────────────────────
typedef enum {
    MLX90393_OSR_1  = 0x00,
    MLX90393_OSR_2  = 0x01,
    MLX90393_OSR_4  = 0x02,
    MLX90393_OSR_8  = 0x03,
} mlx90393_osr_t;

// ── Digital Filter ──────────────────────────────────────────────────
typedef enum {
    MLX90393_FILTER_1  = 0x00,
    MLX90393_FILTER_2  = 0x01,
    MLX90393_FILTER_3  = 0x02,
    MLX90393_FILTER_4  = 0x03,
    MLX90393_FILTER_5  = 0x04,
    MLX90393_FILTER_6  = 0x05,
    MLX90393_FILTER_7  = 0x06,
    MLX90393_FILTER_8  = 0x07,
} mlx90393_filter_t;

// ── Resolution ──────────────────────────────────────────────────────
typedef enum {
    MLX90393_RES_16 = 0x00,
    MLX90393_RES_17 = 0x01,
    MLX90393_RES_18 = 0x02,
    MLX90393_RES_19 = 0x03,
} mlx90393_resolution_t;

// ── Raw Measurement Data ────────────────────────────────────────────
typedef struct {
    float    x_uT;         /**< X-axis magnetic field in microtesla */
    float    y_uT;         /**< Y-axis magnetic field in microtesla */
    float    z_uT;         /**< Z-axis magnetic field in microtesla */
    float    temp_c;       /**< Temperature in Celsius */
    uint32_t timestamp_ms; /**< Millisecond timestamp (app_timer or RTC) */
    bool     valid;        /**< True if measurement is valid */
} mlx90393_measurement_t;

// ── Device Configuration ────────────────────────────────────────────
typedef struct {
    const nrfx_twim_t *twim;           /**< Pointer to nrfx TWIM instance */
    uint8_t            i2c_addr;
    mlx90393_gain_t    gain;
    mlx90393_osr_t     osr;
    mlx90393_filter_t  filter;
    mlx90393_resolution_t resolution_xyz;
} mlx90393_config_t;

// ── Device Handle ───────────────────────────────────────────────────
typedef struct {
    mlx90393_config_t config;
    bool     initialised;
    uint8_t  status;
} mlx90393_dev_t;

// ── Default Configuration (tuned for Zemi wearable) ────────────────
// Gain 1X + OSR 4 + Filter 5 gives ~50 Hz throughput with low noise.
// Adjust for the nRF52840 TWIM instance and GPIO pin assignment in
// board-specific init (see main.c).
#define MLX90393_DEFAULT_CONFIG(_twim_instance_ptr) {   \
    .twim           = (_twim_instance_ptr),              \
    .i2c_addr       = MLX90393_I2C_ADDR_A,              \
    .gain           = MLX90393_GAIN_1X,                  \
    .osr            = MLX90393_OSR_4,                    \
    .filter         = MLX90393_FILTER_5,                 \
    .resolution_xyz = MLX90393_RES_17,                   \
}

// ── Public API ───────────────────────────────────────────────────────

/**
 * @brief Initialise the MLX90393 sensor.
 * @param dev    Device handle (caller-allocated).
 * @param config Driver configuration.
 * @return NRF_SUCCESS, or nrfx error code.
 */
ret_code_t mlx90393_init(mlx90393_dev_t *dev, const mlx90393_config_t *config);

/**
 * @brief Reset the sensor to power-on defaults.
 */
ret_code_t mlx90393_reset(mlx90393_dev_t *dev);

/**
 * @brief Trigger and read one measurement (blocking, waits for conversion).
 * @param axes   Bitmask of MLX90393_AXIS_X/Y/Z/T.
 * @param out    Populated on NRF_SUCCESS.
 */
ret_code_t mlx90393_read_single(mlx90393_dev_t *dev, uint8_t axes,
                                 mlx90393_measurement_t *out);

/**
 * @brief Start continuous burst mode at the configured data rate.
 */
ret_code_t mlx90393_start_burst(mlx90393_dev_t *dev, uint8_t axes);

/**
 * @brief Read latest burst measurement (non-blocking, call at sample rate).
 */
ret_code_t mlx90393_read_burst(mlx90393_dev_t *dev,
                                mlx90393_measurement_t *out);

/**
 * @brief Exit burst/single mode and return sensor to idle.
 */
ret_code_t mlx90393_exit(mlx90393_dev_t *dev);

/**
 * @brief Compute the 3D magnetic field magnitude (µT).
 */
float mlx90393_magnitude(const mlx90393_measurement_t *m);

#ifdef __cplusplus
}
#endif

#endif /* MLX90393_H */
