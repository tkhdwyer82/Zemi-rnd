/**
 * @file mlx90393.c
 * @brief MLX90393 3D Magnetic Sensor Driver — nRF52 Implementation
 *
 * Zemi R&D 2026 — nRF52 Wearable Firmware Scaffold
 * Contractor: Timothy Dwyer (TKD Research and Consulting)
 * Company:    Zemi Pty Ltd
 */

#include "mlx90393.h"
#include "nrf_log.h"
#include "nrf_delay.h"
#include <math.h>
#include <string.h>

// ── Sensitivity lookup (µT/LSB) at each GAIN setting, RES=17 ────────
// Source: MLX90393 datasheet Table 18
static const float SENS_XY_uT[8] = {
    0.805f, 0.644f, 0.483f, 0.403f,
    0.322f, 0.268f, 0.215f, 0.161f
};
static const float SENS_Z_uT[8] = {
    1.468f, 1.174f, 0.881f, 0.734f,
    0.587f, 0.489f, 0.391f, 0.293f
};

#define TEMP_LSB_PER_DEG    45.2f
#define TEMP_OFFSET_LSB     340.0f
#define TEMP_REF_DEG        25.0f

// ── External millisecond tick ────────────────────────────────────────
// Provide this via app_timer or SysTick in your BSP. Declared weak so
// the default stub links if no BSP layer is present during unit tests.
__attribute__((weak)) uint32_t zemi_get_tick_ms(void) { return 0; }

// ── Internal helpers ─────────────────────────────────────────────────

static ret_code_t _twim_write(const mlx90393_dev_t *dev,
                               const uint8_t *data, size_t len)
{
    nrfx_twim_xfer_desc_t xfer =
        NRFX_TWIM_XFER_DESC_TX(dev->config.i2c_addr, (uint8_t *)data, len);
    return nrfx_twim_xfer(dev->config.twim, &xfer, 0);
}

static ret_code_t _twim_read(const mlx90393_dev_t *dev,
                              uint8_t *data, size_t len)
{
    nrfx_twim_xfer_desc_t xfer =
        NRFX_TWIM_XFER_DESC_RX(dev->config.i2c_addr, data, len);
    return nrfx_twim_xfer(dev->config.twim, &xfer, 0);
}

/**
 * Send one command byte and read back the status byte.
 */
static ret_code_t _send_command(mlx90393_dev_t *dev,
                                 uint8_t cmd_byte, uint8_t *status_out)
{
    ret_code_t err;
    err = _twim_write(dev, &cmd_byte, 1);
    if (err != NRF_SUCCESS) return err;

    uint8_t rx = 0;
    err = _twim_read(dev, &rx, 1);
    if (status_out) *status_out = rx;
    return err;
}

static inline int16_t _raw16(uint8_t msb, uint8_t lsb)
{
    return (int16_t)((uint16_t)msb << 8 | lsb);
}

/**
 * Write one configuration register (WR command: 0x60, hi byte, lo byte, addr<<2).
 */
static ret_code_t _write_reg(mlx90393_dev_t *dev,
                               uint8_t reg, uint8_t val_hi, uint8_t val_lo)
{
    uint8_t buf[4] = {
        MLX90393_CMD_WR,
        val_hi,
        val_lo,
        (uint8_t)(reg << 2)
    };
    ret_code_t err = _twim_write(dev, buf, sizeof(buf));
    if (err != NRF_SUCCESS) return err;

    uint8_t status;
    return _twim_read(dev, &status, 1);
}

static ret_code_t _apply_config(mlx90393_dev_t *dev)
{
    // REG_0 bits [6:4] = GAIN_SEL
    uint8_t reg0_lo = (uint8_t)(dev->config.gain & 0x07) << 4;

    // REG_2: RES_XYZ [7:4], DIG_FILT [3:2], OSR [1:0]
    // For RES_17: bits for X (7:6), Y (5:4), Z (3:2 of high nibble) — simplified:
    // RES for all three axes packed as: RES_X[1:0]=bits[5:4], RES_Y[1:0]=bits[3:2],
    // but MLX90393 REG_2 structure packs X and Y, REG_3 packs Z. We simplify by
    // writing the same resolution for all axes across both bytes.
    uint8_t res = dev->config.resolution_xyz & 0x03;
    uint8_t flt = dev->config.filter        & 0x07;
    uint8_t osr = dev->config.osr           & 0x03;

    // REG_2 high byte: RES_X[1:0] in bits [5:4], RES_Y[1:0] in bits [3:2]
    uint8_t reg2_hi = (uint8_t)((res << 5) | (res << 3));
    // REG_2 low byte: RES_Z[1:0] in bits [7:6], DIG_FILT[2:0] in [4:2], OSR[1:0] in [1:0]
    uint8_t reg2_lo = (uint8_t)((res << 6) | (flt << 2) | osr);

    ret_code_t err;
    err = _write_reg(dev, MLX90393_REG_0, 0x00, reg0_lo);
    if (err != NRF_SUCCESS) return err;
    err = _write_reg(dev, MLX90393_REG_2, reg2_hi, reg2_lo);
    return err;
}

static void _parse_measurement(const mlx90393_dev_t *dev,
                                const uint8_t *buf, uint8_t axes,
                                mlx90393_measurement_t *out)
{
    uint8_t gain = dev->config.gain & 0x07;
    float sx = SENS_XY_uT[gain];
    float sy = SENS_XY_uT[gain];
    float sz = SENS_Z_uT[gain];

    int idx = 1; // buf[0] = status byte
    out->valid        = true;
    out->timestamp_ms = zemi_get_tick_ms();

    if (axes & MLX90393_AXIS_T) {
        int16_t t_raw = _raw16(buf[idx], buf[idx+1]);
        idx += 2;
        out->temp_c = TEMP_REF_DEG + (t_raw - TEMP_OFFSET_LSB) / TEMP_LSB_PER_DEG;
    } else {
        out->temp_c = 0.0f;
    }
    if (axes & MLX90393_AXIS_X) {
        int16_t raw = _raw16(buf[idx], buf[idx+1]);
        idx += 2;
        out->x_uT = raw * sx;
    }
    if (axes & MLX90393_AXIS_Y) {
        int16_t raw = _raw16(buf[idx], buf[idx+1]);
        idx += 2;
        out->y_uT = raw * sy;
    }
    if (axes & MLX90393_AXIS_Z) {
        int16_t raw = _raw16(buf[idx], buf[idx+1]);
        out->z_uT = raw * sz;
    }
}

// ── Public API ────────────────────────────────────────────────────────

ret_code_t mlx90393_init(mlx90393_dev_t *dev, const mlx90393_config_t *config)
{
    if (!dev || !config || !config->twim) return NRF_ERROR_INVALID_PARAM;
    memcpy(&dev->config, config, sizeof(mlx90393_config_t));
    dev->initialised = false;
    dev->status      = 0;

    ret_code_t err = mlx90393_reset(dev);
    if (err != NRF_SUCCESS) {
        NRF_LOG_ERROR("MLX90393: reset failed (err=%d)", err);
        return err;
    }
    nrf_delay_ms(10); // post-reset settle

    err = _apply_config(dev);
    if (err != NRF_SUCCESS) {
        NRF_LOG_ERROR("MLX90393: config apply failed (err=%d)", err);
        return err;
    }

    dev->initialised = true;
    NRF_LOG_INFO("MLX90393 init OK addr=0x%02X gain=%d osr=%d",
                 config->i2c_addr, config->gain, config->osr);
    return NRF_SUCCESS;
}

ret_code_t mlx90393_reset(mlx90393_dev_t *dev)
{
    uint8_t status;
    ret_code_t err = _send_command(dev, MLX90393_CMD_RT, &status);
    if (err == NRF_SUCCESS) {
        dev->status = status;
        NRF_LOG_DEBUG("MLX90393 reset status=0x%02X", status);
    }
    return err;
}

ret_code_t mlx90393_read_single(mlx90393_dev_t *dev, uint8_t axes,
                                 mlx90393_measurement_t *out)
{
    if (!dev->initialised) return NRF_ERROR_INVALID_STATE;
    memset(out, 0, sizeof(*out));

    uint8_t sm_cmd = MLX90393_CMD_SM | (axes & 0x0F);
    uint8_t status;
    ret_code_t err = _send_command(dev, sm_cmd, &status);
    if (err != NRF_SUCCESS) return err;

    // Conservative conversion wait: base 10 ms + 2^OSR * 2^FILTER ms
    uint32_t wait_ms = 10u + (1u << dev->config.osr) * (1u << dev->config.filter);
    nrf_delay_ms(wait_ms);

    uint8_t num_axes = (uint8_t)__builtin_popcount(axes & MLX90393_AXIS_XYZT);
    uint8_t buf_len  = 1u + num_axes * 2u;
    uint8_t buf[9]   = {0};

    uint8_t rm_cmd = MLX90393_CMD_RM | (axes & 0x0F);
    err = _twim_write(dev, &rm_cmd, 1);
    if (err != NRF_SUCCESS) return err;
    err = _twim_read(dev, buf, buf_len);
    if (err != NRF_SUCCESS) return err;

    dev->status = buf[0];
    if (dev->status & 0x10) {
        NRF_LOG_WARNING("MLX90393: status error bit set 0x%02X", dev->status);
        out->valid = false;
        return NRF_ERROR_INVALID_DATA;
    }

    _parse_measurement(dev, buf, axes, out);
    return NRF_SUCCESS;
}

ret_code_t mlx90393_start_burst(mlx90393_dev_t *dev, uint8_t axes)
{
    if (!dev->initialised) return NRF_ERROR_INVALID_STATE;
    uint8_t sb_cmd = MLX90393_CMD_SB | (axes & 0x0F);
    uint8_t status;
    ret_code_t err = _send_command(dev, sb_cmd, &status);
    dev->status = status;
    NRF_LOG_INFO("MLX90393 burst started axes=0x%02X status=0x%02X", axes, status);
    return err;
}

ret_code_t mlx90393_read_burst(mlx90393_dev_t *dev,
                                mlx90393_measurement_t *out)
{
    if (!dev->initialised) return NRF_ERROR_INVALID_STATE;
    memset(out, 0, sizeof(*out));

    const uint8_t axes   = MLX90393_AXIS_XYZT;
    uint8_t       buf[9] = {0};

    uint8_t rm_cmd = MLX90393_CMD_RM | (axes & 0x0F);
    ret_code_t err = _twim_write(dev, &rm_cmd, 1);
    if (err != NRF_SUCCESS) return err;
    err = _twim_read(dev, buf, sizeof(buf));
    if (err != NRF_SUCCESS) return err;

    dev->status = buf[0];
    _parse_measurement(dev, buf, axes, out);
    return NRF_SUCCESS;
}

ret_code_t mlx90393_exit(mlx90393_dev_t *dev)
{
    uint8_t status;
    ret_code_t err = _send_command(dev, MLX90393_CMD_EX, &status);
    dev->status = status;
    NRF_LOG_INFO("MLX90393 exited mode status=0x%02X", status);
    return err;
}

float mlx90393_magnitude(const mlx90393_measurement_t *m)
{
    return sqrtf(m->x_uT * m->x_uT + m->y_uT * m->y_uT + m->z_uT * m->z_uT);
}
