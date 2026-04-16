/**
 * @file mlx90393.c
 * @brief MLX90393 3D Magnetic Sensor Driver — Implementation
 *
 * Zemi R&D 2026 — Core Activity 1
 * Contractor: Timothy Dwyer (TKD Research and Consulting)
 * Company:    Zemi Pty Ltd
 */

#include "mlx90393.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "MLX90393";

// ── Sensitivity lookup (uT/LSB) at each gain, RES=17 ───────────────
// Source: MLX90393 datasheet Table 18
static const float SENS_XY_uT[8] = {
    0.805f, 0.644f, 0.483f, 0.403f,
    0.322f, 0.268f, 0.215f, 0.161f
};
static const float SENS_Z_uT[8] = {
    1.468f, 1.174f, 0.881f, 0.734f,
    0.587f, 0.489f, 0.391f, 0.293f
};
// Temperature: fixed 45.2 LSB/°C, offset 340 LSB
#define TEMP_LSB_PER_DEG    45.2f
#define TEMP_OFFSET_LSB     340.0f
#define TEMP_REF_DEG        25.0f

// ── Internal helpers ─────────────────────────────────────────────────

static esp_err_t _i2c_write(mlx90393_dev_t *dev,
                              const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd,
        (dev->config.i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, (uint8_t *)data, len, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(dev->config.i2c_port, cmd,
                        pdMS_TO_TICKS(MLX90393_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t _i2c_read(mlx90393_dev_t *dev,
                             uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd,
        (dev->config.i2c_addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &data[len - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(dev->config.i2c_port, cmd,
                        pdMS_TO_TICKS(MLX90393_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t _send_command(mlx90393_dev_t *dev,
                                uint8_t cmd_byte, uint8_t *status_out)
{
    uint8_t tx = cmd_byte;
    uint8_t rx = 0;
    esp_err_t ret = _i2c_write(dev, &tx, 1);
    if (ret != ESP_OK) return ret;
    ret = _i2c_read(dev, &rx, 1);
    if (status_out) *status_out = rx;
    return ret;
}

static int16_t _raw16(uint8_t msb, uint8_t lsb)
{
    return (int16_t)((uint16_t)msb << 8 | lsb);
}

static esp_err_t _apply_config(mlx90393_dev_t *dev)
{
    // REG_0: GAIN_SEL (bits 6:4)
    uint8_t reg0 = (uint8_t)(dev->config.gain & 0x07) << 4;

    // REG_2: RES_XYZ (bits 7:4), DIG_FILT (bits 3:2), OSR (bits 1:0)
    uint8_t reg2 = (uint8_t)((dev->config.resolution_xyz & 0x03) << 5)
                 | (uint8_t)((dev->config.resolution_xyz & 0x03) << 3)  // same for Y
                 | (uint8_t)((dev->config.resolution_xyz & 0x03) << 1); // same for Z
    // Set DIG_FILT and OSR in lower nibble
    uint8_t reg2_low = (uint8_t)((dev->config.filter & 0x07) << 2)
                     | (uint8_t)(dev->config.osr & 0x03);
    reg2 |= reg2_low;

    uint8_t wr_reg0[4] = {
        MLX90393_CMD_WR, 0x00, reg0, (uint8_t)(MLX90393_REG_0 << 2)
    };
    uint8_t wr_reg2[4] = {
        MLX90393_CMD_WR, 0x00, reg2, (uint8_t)(MLX90393_REG_2 << 2)
    };

    esp_err_t ret;
    uint8_t status;

    ret = _i2c_write(dev, wr_reg0, sizeof(wr_reg0));
    if (ret != ESP_OK) return ret;
    ret = _i2c_read(dev, &status, 1);
    if (ret != ESP_OK) return ret;

    ret = _i2c_write(dev, wr_reg2, sizeof(wr_reg2));
    if (ret != ESP_OK) return ret;
    ret = _i2c_read(dev, &status, 1);
    dev->status = status;
    return ret;
}

static void _parse_measurement(mlx90393_dev_t *dev,
                                const uint8_t *buf, uint8_t axes,
                                mlx90393_measurement_t *out)
{
    uint8_t gain = dev->config.gain & 0x07;
    float sx = SENS_XY_uT[gain];
    float sy = SENS_XY_uT[gain];
    float sz = SENS_Z_uT[gain];

    int idx = 1; // buf[0] = status byte
    out->valid     = true;
    out->timestamp = esp_timer_get_time();

    if (axes & MLX90393_AXIS_T) {
        int16_t t_raw = _raw16(buf[idx], buf[idx+1]);
        idx += 2;
        out->temp_c = TEMP_REF_DEG +
            (t_raw - TEMP_OFFSET_LSB) / TEMP_LSB_PER_DEG;
    } else {
        out->temp_c = 0.0f;
    }
    if (axes & MLX90393_AXIS_X) {
        int16_t x_raw = _raw16(buf[idx], buf[idx+1]);
        idx += 2;
        out->x_uT = x_raw * sx;
    }
    if (axes & MLX90393_AXIS_Y) {
        int16_t y_raw = _raw16(buf[idx], buf[idx+1]);
        idx += 2;
        out->y_uT = y_raw * sy;
    }
    if (axes & MLX90393_AXIS_Z) {
        int16_t z_raw = _raw16(buf[idx], buf[idx+1]);
        out->z_uT = z_raw * sz;
    }
}

// ── Public API ────────────────────────────────────────────────────────

esp_err_t mlx90393_init(mlx90393_dev_t *dev, const mlx90393_config_t *config)
{
    if (!dev || !config) return ESP_ERR_INVALID_ARG;
    memcpy(&dev->config, config, sizeof(mlx90393_config_t));
    dev->initialised = false;
    dev->status = 0;

    esp_err_t ret = mlx90393_reset(dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Reset failed: %s", esp_err_to_name(ret));
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // post-reset settle

    ret = _apply_config(dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Config apply failed: %s", esp_err_to_name(ret));
        return ret;
    }

    dev->initialised = true;
    ESP_LOGI(TAG, "MLX90393 initialised at addr 0x%02X, gain=%d, osr=%d",
             config->i2c_addr, config->gain, config->osr);
    return ESP_OK;
}

esp_err_t mlx90393_reset(mlx90393_dev_t *dev)
{
    uint8_t status;
    esp_err_t ret = _send_command(dev, MLX90393_CMD_RT, &status);
    if (ret == ESP_OK) {
        dev->status = status;
        ESP_LOGD(TAG, "Reset status: 0x%02X", status);
    }
    return ret;
}

esp_err_t mlx90393_read_single(mlx90393_dev_t *dev, uint8_t axes,
                                mlx90393_measurement_t *out)
{
    if (!dev->initialised) return ESP_ERR_INVALID_STATE;
    memset(out, 0, sizeof(*out));

    // Send SM command with axis selection
    uint8_t sm_cmd = MLX90393_CMD_SM | (axes & 0x0F);
    uint8_t status;
    esp_err_t ret = _send_command(dev, sm_cmd, &status);
    if (ret != ESP_OK) return ret;

    // Measurement time: depends on OSR and filter — wait conservatively
    uint32_t wait_ms = 10 + (1 << dev->config.osr) * (1 << dev->config.filter);
    vTaskDelay(pdMS_TO_TICKS(wait_ms));

    // Build RM command and read result
    uint8_t num_axes = __builtin_popcount(axes & MLX90393_AXIS_XYZT);
    uint8_t buf_len  = 1 + num_axes * 2; // 1 status + 2 bytes/axis
    uint8_t buf[9]   = {0};

    uint8_t rm_cmd = MLX90393_CMD_RM | (axes & 0x0F);
    ret = _i2c_write(dev, &rm_cmd, 1);
    if (ret != ESP_OK) return ret;
    ret = _i2c_read(dev, buf, buf_len);
    if (ret != ESP_OK) return ret;

    dev->status = buf[0];
    if (dev->status & 0x10) {
        ESP_LOGW(TAG, "Status error bit set: 0x%02X", dev->status);
        out->valid = false;
        return ESP_ERR_INVALID_RESPONSE;
    }

    _parse_measurement(dev, buf, axes, out);
    return ESP_OK;
}

esp_err_t mlx90393_start_burst(mlx90393_dev_t *dev, uint8_t axes)
{
    if (!dev->initialised) return ESP_ERR_INVALID_STATE;
    uint8_t sb_cmd = MLX90393_CMD_SB | (axes & 0x0F);
    uint8_t status;
    esp_err_t ret = _send_command(dev, sb_cmd, &status);
    dev->status = status;
    ESP_LOGI(TAG, "Burst mode started, axes=0x%02X, status=0x%02X", axes, status);
    return ret;
}

esp_err_t mlx90393_read_burst(mlx90393_dev_t *dev,
                               mlx90393_measurement_t *out)
{
    if (!dev->initialised) return ESP_ERR_INVALID_STATE;
    memset(out, 0, sizeof(*out));

    uint8_t axes = MLX90393_AXIS_XYZT;
    uint8_t buf[9] = {0};
    uint8_t buf_len = 9; // max: status + T + X + Y + Z = 1+8

    uint8_t rm_cmd = MLX90393_CMD_RM | (axes & 0x0F);
    esp_err_t ret = _i2c_write(dev, &rm_cmd, 1);
    if (ret != ESP_OK) return ret;
    ret = _i2c_read(dev, buf, buf_len);
    if (ret != ESP_OK) return ret;

    dev->status = buf[0];
    _parse_measurement(dev, buf, axes, out);
    return ESP_OK;
}

esp_err_t mlx90393_exit(mlx90393_dev_t *dev)
{
    uint8_t status;
    esp_err_t ret = _send_command(dev, MLX90393_CMD_EX, &status);
    dev->status = status;
    ESP_LOGI(TAG, "Exited mode, status=0x%02X", status);
    return ret;
}

float mlx90393_magnitude(const mlx90393_measurement_t *m)
{
    return sqrtf(m->x_uT * m->x_uT +
                 m->y_uT * m->y_uT +
                 m->z_uT * m->z_uT);
}
