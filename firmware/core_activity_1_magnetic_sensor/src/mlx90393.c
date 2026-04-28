/**
 * mlx90393.c — MLX90393 3D Magnetic Sensor I2C Driver
 * Zemi R&D 2026 — Core Activity 1: 3D Magnetic Sensor Production-Readiness
 *
 * Target MCU : ESP32-S3
 * Interface  : I2C (400 kHz fast mode)
 * Sensor     : Melexis MLX90393 triaxial magnetic sensor
 *
 * This driver provides raw X/Y/Z magnetic field readings in microtesla (µT)
 * and forms the hardware layer beneath the noise_filter.c and
 * pairing_state_machine.c modules for CA1 experimentation.
 *
 * R&D Note: The outcome of achieving stable pairing across real-world
 * multi-device environments could not be determined in advance. This driver
 * establishes the baseline measurement capability from which iterative
 * firmware experiments (Iterations A–D in noise_filter.c) are conducted.
 *
 * ATO R&D Substantiation: See docs/experiment_log.md — Experiment 1.1
 * GitHub: tkhdwyer82/Zemi-rnd
 * Path:   firmware/core_activity_1_magnetic_sensor/src/mlx90393.c
 */

#include "mlx90393.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "MLX90393";

/* -----------------------------------------------------------------------
 * MLX90393 Register Map & Command Bytes
 * ----------------------------------------------------------------------- */
#define MLX90393_CMD_SB        0x10   /* Start Burst mode       */
#define MLX90393_CMD_SW        0x20   /* Start Wake-on-Change   */
#define MLX90393_CMD_SM        0x30   /* Start Single Measure   */
#define MLX90393_CMD_RM        0x40   /* Read Measurement       */
#define MLX90393_CMD_RR        0x50   /* Read Register          */
#define MLX90393_CMD_WR        0x60   /* Write Register         */
#define MLX90393_CMD_EX        0x80   /* Exit mode              */
#define MLX90393_CMD_RT        0xF0   /* Reset                  */
#define MLX90393_CMD_NOP       0x00   /* No-op / status read    */

/* Axis selection flags ORed into SM/RM commands */
#define MLX90393_AXIS_X        0x02
#define MLX90393_AXIS_Y        0x04
#define MLX90393_AXIS_Z        0x08
#define MLX90393_AXIS_ALL      (MLX90393_AXIS_X | MLX90393_AXIS_Y | MLX90393_AXIS_Z)

/* Register addresses (used with RR/WR commands, address in bits [5:2]) */
#define MLX90393_REG_0         0x00   /* HALLCONF, GAIN_SEL, Z_SERIES */
#define MLX90393_REG_1         0x01   /* BIST, SB, Z_SERIES, GAIN     */
#define MLX90393_REG_2         0x02   /* OSR, DIG_FILT, RES_XYZ        */

/* Gain values (GAIN_SEL field in REG_0, bits [6:4]) */
#define MLX90393_GAIN_5X       0x04   /* Default — mid sensitivity     */
#define MLX90393_GAIN_1X       0x00   /* Lowest sensitivity, widest range */

/* Sensitivity at GAIN_5X, RES=0 (µT per LSB) — from MLX90393 datasheet */
#define SENS_XY_UT_PER_LSB     0.161f
#define SENS_Z_UT_PER_LSB      0.294f

/* I2C timing */
#define I2C_TIMEOUT_MS         50
#define RESET_DELAY_MS         10
#define MEASURE_DELAY_MS       20    /* ≥ t_conv at OSR=0, FILT=0 */

/* -----------------------------------------------------------------------
 * Internal helper — raw I2C write/read
 * ----------------------------------------------------------------------- */
static esp_err_t i2c_write_byte(mlx90393_t *dev, uint8_t byte)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, byte, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return err;
}

static esp_err_t i2c_read_bytes(mlx90393_t *dev, uint8_t *buf, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->i2c_addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, buf, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &buf[len - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return err;
}

/* Send command byte and read back status byte */
static esp_err_t mlx_command(mlx90393_t *dev, uint8_t cmd_byte, uint8_t *status_out)
{
    esp_err_t err = i2c_write_byte(dev, cmd_byte);
    if (err != ESP_OK) return err;
    uint8_t status = 0;
    err = i2c_read_bytes(dev, &status, 1);
    if (status_out) *status_out = status;
    /* Bit 4 (ERROR flag) set indicates sensor error */
    if (status & 0x10) {
        ESP_LOGW(TAG, "MLX90393 status error flag set: 0x%02X", status);
        return ESP_FAIL;
    }
    return err;
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

esp_err_t mlx90393_init(mlx90393_t *dev, i2c_port_t port, uint8_t addr)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    memset(dev, 0, sizeof(mlx90393_t));
    dev->i2c_port = port;
    dev->i2c_addr = addr;

    /* Reset sensor to known state */
    esp_err_t err = mlx90393_reset(dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Reset failed: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(RESET_DELAY_MS));

    /* Configure: GAIN_SEL=5x, OSR=0, FILTER=0, RES_XYZ=0 (baseline for Iteration A) */
    /* Write register 0: GAIN_SEL bits [6:4] = 0x04 → 5x gain */
    uint8_t wr_cmd[3];
    wr_cmd[0] = MLX90393_CMD_WR;
    wr_cmd[1] = 0x00;                /* MSB of register value */
    wr_cmd[2] = (MLX90393_GAIN_5X << 4) | MLX90393_REG_0;  /* address in bits[5:2] */

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, wr_cmd, 3, true);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Config write failed: %s", esp_err_to_name(err));
        return err;
    }

    dev->initialised = true;
    ESP_LOGI(TAG, "MLX90393 initialised at I2C addr 0x%02X (port %d)", addr, port);
    return ESP_OK;
}

esp_err_t mlx90393_reset(mlx90393_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    /* EX first to exit any active mode, then RT */
    i2c_write_byte(dev, MLX90393_CMD_EX);
    vTaskDelay(pdMS_TO_TICKS(1));
    uint8_t status = 0;
    esp_err_t err = mlx_command(dev, MLX90393_CMD_RT, &status);
    ESP_LOGI(TAG, "Reset status: 0x%02X", status);
    return err;
}

esp_err_t mlx90393_read_xyz(mlx90393_t *dev, mlx90393_data_t *data)
{
    if (!dev || !data || !dev->initialised) return ESP_ERR_INVALID_STATE;

    /* Step 1: Trigger single measurement on X+Y+Z */
    uint8_t status = 0;
    esp_err_t err = mlx_command(dev, MLX90393_CMD_SM | MLX90393_AXIS_ALL, &status);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SM command failed");
        return err;
    }

    /* Wait for conversion (t_conv ≈ 1.6 ms at OSR=0, FILT=0; we wait 20ms to be safe) */
    vTaskDelay(pdMS_TO_TICKS(MEASURE_DELAY_MS));

    /* Step 2: Read measurement — returns status + 2 bytes per axis (6 data bytes + 1 status) */
    uint8_t rx[7] = {0};
    /* Send RM command first, then read back */
    err = i2c_write_byte(dev, MLX90393_CMD_RM | MLX90393_AXIS_ALL);
    if (err != ESP_OK) return err;
    err = i2c_read_bytes(dev, rx, 7);
    if (err != ESP_OK) return err;

    /* rx[0] = status, rx[1:2] = X, rx[3:4] = Y, rx[5:6] = Z */
    if (rx[0] & 0x10) {
        ESP_LOGW(TAG, "RM status error: 0x%02X", rx[0]);
        return ESP_FAIL;
    }

    int16_t raw_x = (int16_t)((rx[1] << 8) | rx[2]);
    int16_t raw_y = (int16_t)((rx[3] << 8) | rx[4]);
    int16_t raw_z = (int16_t)((rx[5] << 8) | rx[6]);

    /* Convert raw ADC counts to µT using datasheet sensitivity */
    data->x_ut = (float)raw_x * SENS_XY_UT_PER_LSB;
    data->y_ut = (float)raw_y * SENS_XY_UT_PER_LSB;
    data->z_ut = (float)raw_z * SENS_Z_UT_PER_LSB;

    /* Compute total field magnitude */
    data->magnitude_ut = sqrtf(data->x_ut * data->x_ut +
                                data->y_ut * data->y_ut +
                                data->z_ut * data->z_ut);

    ESP_LOGD(TAG, "X=%.2f Y=%.2f Z=%.2f |B|=%.2f µT",
             data->x_ut, data->y_ut, data->z_ut, data->magnitude_ut);

    return ESP_OK;
}

esp_err_t mlx90393_set_gain(mlx90393_t *dev, uint8_t gain_sel)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    /* gain_sel: 0–7, written to REG_0 bits [6:4] */
    uint8_t buf[3];
    buf[0] = MLX90393_CMD_WR;
    buf[1] = 0x00;
    buf[2] = ((gain_sel & 0x07) << 4) | MLX90393_REG_0;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, buf, 3, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return err;
}
