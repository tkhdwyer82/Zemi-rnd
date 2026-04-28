/**
 * mlx90393.h — MLX90393 3D Magnetic Sensor Driver Header
 * Zemi R&D 2026 — Core Activity 1
 */

#pragma once
#include "driver/i2c.h"
#include "esp_err.h"
#include <stdbool.h>

#define MLX90393_DEFAULT_ADDR  0x0C   /* A0=0, A1=0 (default I2C address) */

/** Raw + converted reading from one measurement cycle */
typedef struct {
    float x_ut;           /**< X-axis field in microtesla */
    float y_ut;           /**< Y-axis field in microtesla */
    float z_ut;           /**< Z-axis field in microtesla */
    float magnitude_ut;   /**< Total field magnitude √(x²+y²+z²) in µT */
} mlx90393_data_t;

/** Device handle */
typedef struct {
    i2c_port_t i2c_port;
    uint8_t    i2c_addr;
    bool       initialised;
} mlx90393_t;

/**
 * @brief  Initialise sensor, reset to defaults, apply baseline gain config.
 * @param  dev   Pointer to device handle (allocated by caller)
 * @param  port  ESP32 I2C port number (I2C_NUM_0 or I2C_NUM_1)
 * @param  addr  7-bit I2C address (use MLX90393_DEFAULT_ADDR = 0x0C)
 * @return ESP_OK on success
 */
esp_err_t mlx90393_init(mlx90393_t *dev, i2c_port_t port, uint8_t addr);

/**
 * @brief  Send EX then RT to reset sensor to power-on defaults.
 */
esp_err_t mlx90393_reset(mlx90393_t *dev);

/**
 * @brief  Trigger a single measurement and read X, Y, Z fields.
 *         Blocks for ~20 ms during conversion.
 * @param  data  Output struct populated with µT values and magnitude.
 * @return ESP_OK on success; ESP_FAIL if sensor error flag is set.
 */
esp_err_t mlx90393_read_xyz(mlx90393_t *dev, mlx90393_data_t *data);

/**
 * @brief  Change the analogue gain. Called by noise filter iterations
 *         that vary gain as a secondary variable.
 * @param  gain_sel  0–7 (datasheet Table 18); 4 = 5× (default baseline).
 */
esp_err_t mlx90393_set_gain(mlx90393_t *dev, uint8_t gain_sel);
