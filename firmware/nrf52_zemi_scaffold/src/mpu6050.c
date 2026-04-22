/**
 * @file mpu6050.c
 * @brief MPU-6050 6-DOF IMU Driver + Gesture Detector — nRF52 Implementation
 *
 * Zemi R&D 2026 — nRF52 Wearable Firmware Scaffold
 * Contractor: Timothy Dwyer (TKD Research and Consulting)
 * Company:    Zemi Pty Ltd
 *
 * Gesture detection design notes
 * ───────────────────────────────
 * All three detectors run as lightweight state machines called at 100 Hz.
 * No heap allocation; all state is in the gesture_detector_t struct.
 *
 * ALIGN
 *   Gravity must be within ±GESTURE_ALIGN_GRAVITY_TOL_G of a unit vector
 *   along any primary axis, AND gyro must be below GESTURE_ALIGN_GYRO_TOL_DPS.
 *   After GESTURE_ALIGN_STABLE_MS consecutive qualifying samples, ALIGN fires.
 *
 * SINGLE_WAVE
 *   Lateral acceleration (XY plane magnitude) crosses GESTURE_WAVE_ACCEL_THRESHOLD_G,
 *   then returns below GESTURE_WAVE_RETURN_THRESHOLD_G within GESTURE_WAVE_MAX_DURATION_MS.
 *   One complete peak-and-return = one wave.
 *
 * WRIST_FLICK
 *   Total gyro magnitude exceeds GESTURE_FLICK_GYRO_THRESHOLD_DPS
 *   and the spike resolves within GESTURE_FLICK_MAX_DURATION_MS.
 */

#include "mpu6050.h"
#include "nrf_log.h"
#include "nrf_delay.h"
#include <math.h>
#include <string.h>

// ── External millisecond tick (provided by BSP, weak stub for tests) ─
__attribute__((weak)) uint32_t zemi_get_tick_ms(void) { return 0; }

// ── I2C helpers ──────────────────────────────────────────────────────

static ret_code_t _reg_write(const mpu6050_dev_t *dev,
                               uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    nrfx_twim_xfer_desc_t xfer =
        NRFX_TWIM_XFER_DESC_TX(dev->config.i2c_addr, buf, sizeof(buf));
    return nrfx_twim_xfer(dev->config.twim, &xfer, 0);
}

static ret_code_t _reg_read(const mpu6050_dev_t *dev,
                              uint8_t reg, uint8_t *data, size_t len)
{
    // Register address write, then repeated-start read
    nrfx_twim_xfer_desc_t xfer =
        NRFX_TWIM_XFER_DESC_TXRX(dev->config.i2c_addr, &reg, 1, data, len);
    return nrfx_twim_xfer(dev->config.twim, &xfer, 0);
}

static inline int16_t _raw16(uint8_t hi, uint8_t lo)
{
    return (int16_t)((uint16_t)hi << 8 | lo);
}

// ── Sensitivity LUTs ─────────────────────────────────────────────────

static const float ACCEL_LSB_PER_G[4] = { 16384.0f, 8192.0f, 4096.0f, 2048.0f };
static const float GYRO_LSB_PER_DPS[4] = { 131.0f, 65.5f, 32.8f, 16.4f };

// ── Public API ────────────────────────────────────────────────────────

ret_code_t mpu6050_init(mpu6050_dev_t *dev, const mpu6050_config_t *config)
{
    if (!dev || !config || !config->twim) return NRF_ERROR_INVALID_PARAM;
    memcpy(&dev->config, config, sizeof(mpu6050_config_t));
    dev->initialised = false;

    // Soft reset
    ret_code_t err = mpu6050_reset(dev);
    if (err != NRF_SUCCESS) {
        NRF_LOG_ERROR("MPU6050: reset failed (err=%d)", err);
        return err;
    }
    nrf_delay_ms(100); // datasheet: 100 ms after reset

    // Verify WHO_AM_I
    uint8_t who = 0;
    err = _reg_read(dev, MPU6050_REG_WHO_AM_I, &who, 1);
    if (err != NRF_SUCCESS || who != MPU6050_WHO_AM_I_VAL) {
        NRF_LOG_ERROR("MPU6050: WHO_AM_I=0x%02X (expected 0x68, err=%d)", who, err);
        return (err == NRF_SUCCESS) ? NRF_ERROR_NOT_FOUND : err;
    }

    // Wake from sleep (PWR_MGMT_1 = 0: clock=internal, no sleep, no cycle)
    err = _reg_write(dev, MPU6050_REG_PWR_MGMT_1, 0x00);
    if (err != NRF_SUCCESS) return err;

    // DLPF
    err = _reg_write(dev, MPU6050_REG_CONFIG, config->dlpf & 0x07);
    if (err != NRF_SUCCESS) return err;

    // Sample rate divider
    err = _reg_write(dev, MPU6050_REG_SMPLRT_DIV, config->sample_rate_div);
    if (err != NRF_SUCCESS) return err;

    // Gyro full-scale
    err = _reg_write(dev, MPU6050_REG_GYRO_CONFIG,
                     (uint8_t)((config->gyro_fs & 0x03) << 3));
    if (err != NRF_SUCCESS) return err;

    // Accel full-scale
    err = _reg_write(dev, MPU6050_REG_ACCEL_CONFIG,
                     (uint8_t)((config->accel_fs & 0x03) << 3));
    if (err != NRF_SUCCESS) return err;

    dev->accel_lsb_per_g   = ACCEL_LSB_PER_G[config->accel_fs & 0x03];
    dev->gyro_lsb_per_dps  = GYRO_LSB_PER_DPS[config->gyro_fs & 0x03];
    dev->initialised       = true;

    NRF_LOG_INFO("MPU6050 init OK addr=0x%02X accel_fs=%d gyro_fs=%d rate_div=%d",
                 config->i2c_addr, config->accel_fs, config->gyro_fs,
                 config->sample_rate_div);
    return NRF_SUCCESS;
}

ret_code_t mpu6050_reset(mpu6050_dev_t *dev)
{
    // PWR_MGMT_1 bit 7 = DEVICE_RESET
    ret_code_t err = _reg_write(dev, MPU6050_REG_PWR_MGMT_1, 0x80);
    if (err == NRF_SUCCESS) {
        NRF_LOG_DEBUG("MPU6050 reset issued");
    }
    return err;
}

ret_code_t mpu6050_read(mpu6050_dev_t *dev, mpu6050_data_t *out)
{
    if (!dev->initialised) return NRF_ERROR_INVALID_STATE;

    // Read 14 bytes: ACCEL_XOUT_H through GYRO_ZOUT_L (registers 0x3B..0x48)
    uint8_t buf[14];
    ret_code_t err = _reg_read(dev, MPU6050_REG_ACCEL_XOUT_H, buf, sizeof(buf));
    if (err != NRF_SUCCESS) {
        out->valid = false;
        return err;
    }

    float s_a = dev->accel_lsb_per_g;
    float s_g = dev->gyro_lsb_per_dps;

    out->ax_g       = _raw16(buf[0],  buf[1])  / s_a;
    out->ay_g       = _raw16(buf[2],  buf[3])  / s_a;
    out->az_g       = _raw16(buf[4],  buf[5])  / s_a;
    // bytes 6,7 = temperature
    int16_t t_raw   = _raw16(buf[6],  buf[7]);
    out->temp_c     = (t_raw / 340.0f) + 36.53f; // MPU-6050 datasheet formula
    out->gx_dps     = _raw16(buf[8],  buf[9])  / s_g;
    out->gy_dps     = _raw16(buf[10], buf[11]) / s_g;
    out->gz_dps     = _raw16(buf[12], buf[13]) / s_g;
    out->timestamp_ms = zemi_get_tick_ms();
    out->valid      = true;
    return NRF_SUCCESS;
}

// ── Gesture Detector ─────────────────────────────────────────────────

void gesture_detector_init(gesture_detector_t *det)
{
    memset(det, 0, sizeof(*det));
}

/**
 * Determine if the accelerometer vector is close to ±1g on any primary axis.
 * Returns true if the device is level (aligned).
 */
static bool _is_aligned(const mpu6050_data_t *d)
{
    float tol   = GESTURE_ALIGN_GRAVITY_TOL_G;
    float ax    = d->ax_g, ay = d->ay_g, az = d->az_g;

    // One axis must be close to ±1g; the other two must be small
    bool x_dom = (fabsf(fabsf(ax) - 1.0f) < tol) && (fabsf(ay) < tol) && (fabsf(az) < tol);
    bool y_dom = (fabsf(fabsf(ay) - 1.0f) < tol) && (fabsf(ax) < tol) && (fabsf(az) < tol);
    bool z_dom = (fabsf(fabsf(az) - 1.0f) < tol) && (fabsf(ax) < tol) && (fabsf(ay) < tol);

    if (!(x_dom || y_dom || z_dom)) return false;

    // Gyro must also be quiet
    float gyro_mag = sqrtf(d->gx_dps*d->gx_dps +
                            d->gy_dps*d->gy_dps +
                            d->gz_dps*d->gz_dps);
    return (gyro_mag < GESTURE_ALIGN_GYRO_TOL_DPS);
}

/**
 * Lateral acceleration magnitude in the XY plane.
 */
static inline float _lateral_g(const mpu6050_data_t *d)
{
    return sqrtf(d->ax_g * d->ax_g + d->ay_g * d->ay_g);
}

/**
 * Total gyro magnitude.
 */
static inline float _gyro_mag(const mpu6050_data_t *d)
{
    return sqrtf(d->gx_dps*d->gx_dps +
                 d->gy_dps*d->gy_dps +
                 d->gz_dps*d->gz_dps);
}

zemi_gesture_t mpu6050_detect_gesture(gesture_detector_t *det,
                                        const mpu6050_data_t *data,
                                        uint32_t timestamp_ms)
{
    if (!data->valid) return GESTURE_NONE;

    // ── 1. ALIGN ──────────────────────────────────────────────────────
    if (_is_aligned(data)) {
        if (det->align_stable_ms == 0) {
            det->align_stable_ms = timestamp_ms;
        } else {
            uint32_t held_ms = timestamp_ms - det->align_stable_ms;
            if (held_ms >= GESTURE_ALIGN_STABLE_MS) {
                det->align_stable_ms = 0; // reset so it doesn't re-fire immediately
                NRF_LOG_INFO("GESTURE: ALIGN detected");
                return GESTURE_ALIGN;
            }
        }
    } else {
        det->align_stable_ms = 0;
    }

    float lat = _lateral_g(data);

    // ── 2. SINGLE_WAVE ────────────────────────────────────────────────
    if (!det->wave_in_progress) {
        if (lat > GESTURE_WAVE_ACCEL_THRESHOLD_G) {
            det->wave_in_progress = true;
            det->wave_peak_g      = lat;
            det->wave_start_ms    = timestamp_ms;
        }
    } else {
        // Track peak
        if (lat > det->wave_peak_g) det->wave_peak_g = lat;

        uint32_t wave_dur = timestamp_ms - det->wave_start_ms;
        if (wave_dur > GESTURE_WAVE_MAX_DURATION_MS) {
            // Took too long — not a crisp wave
            det->wave_in_progress = false;
        } else if (lat < GESTURE_WAVE_RETURN_THRESHOLD_G) {
            // Arm has returned — wave complete
            det->wave_in_progress = false;
            NRF_LOG_INFO("GESTURE: SINGLE_WAVE detected (peak=%.2fg, dur=%ums)",
                         det->wave_peak_g, wave_dur);
            return GESTURE_SINGLE_WAVE;
        }
    }

    float gmag = _gyro_mag(data);

    // ── 3. WRIST_FLICK ────────────────────────────────────────────────
    if (!det->flick_in_progress) {
        if (gmag > GESTURE_FLICK_GYRO_THRESHOLD_DPS) {
            det->flick_in_progress = true;
            det->flick_peak_dps    = gmag;
            det->flick_start_ms    = timestamp_ms;
        }
    } else {
        if (gmag > det->flick_peak_dps) det->flick_peak_dps = gmag;

        uint32_t flick_dur = timestamp_ms - det->flick_start_ms;
        if (flick_dur > GESTURE_FLICK_MAX_DURATION_MS) {
            // Too slow — sustained rotation, not a flick
            det->flick_in_progress = false;
        } else if (gmag < GESTURE_FLICK_GYRO_THRESHOLD_DPS * 0.4f) {
            // Rotation rate has subsided — flick complete
            det->flick_in_progress = false;
            NRF_LOG_INFO("GESTURE: WRIST_FLICK detected (peak=%.1fdps, dur=%ums)",
                         det->flick_peak_dps, flick_dur);
            return GESTURE_WRIST_FLICK;
        }
    }

    return GESTURE_NONE;
}

const char *gesture_name(zemi_gesture_t g)
{
    switch (g) {
        case GESTURE_NONE:        return "NONE";
        case GESTURE_ALIGN:       return "ALIGN";
        case GESTURE_SINGLE_WAVE: return "SINGLE_WAVE";
        case GESTURE_WRIST_FLICK: return "WRIST_FLICK";
        default:                  return "UNKNOWN";
    }
}
