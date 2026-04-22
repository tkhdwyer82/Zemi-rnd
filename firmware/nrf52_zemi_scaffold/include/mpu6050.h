/**
 * @file mpu6050.h
 * @brief MPU-6050 6-DOF IMU Driver + Gesture Detector — nRF52
 *
 * Zemi R&D 2026 — nRF52 Wearable Firmware Scaffold
 *
 * Contractor: Timothy Dwyer (TKD Research and Consulting)
 * Company:    Zemi Pty Ltd
 * Created:    April 2026
 *
 * Target MCU: nRF52840
 * Interface:  I2C via nrfx_twim (400 kHz)
 * Sensor:     InvenSense MPU-6050
 *             3-axis accelerometer + 3-axis gyroscope
 *
 * Detected Gestures
 * ─────────────────
 *   ALIGN       — Device held level, steady (gravity along one axis,
 *                 near-zero gyro). Used to signal "ready to pair".
 *   SINGLE_WAVE — Quick lateral arc (sharp accel peak then return).
 *                 Used to confirm pairing / dismiss notification.
 *   WRIST_FLICK — Sharp, brief wrist rotation (gyro spike, ~100 deg/s,
 *                 duration < 300 ms). Used to reject pairing / snooze.
 *
 * Detection algorithm:
 *   Each gesture runs a lightweight state machine updated at 100 Hz.
 *   No external ML inference — thresholds tuned against Gen Alpha FY25
 *   movement data. See mpu6050.c for tuning constants.
 */

#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include <stdbool.h>
#include "nrfx_twim.h"
#include "sdk_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── I2C Addresses ────────────────────────────────────────────────────
#define MPU6050_I2C_ADDR_LOW    0x68    /**< AD0 pin = GND */
#define MPU6050_I2C_ADDR_HIGH   0x69    /**< AD0 pin = VCC */

// ── Key Register Map ────────────────────────────────────────────────
#define MPU6050_REG_SELF_TEST_X     0x0D
#define MPU6050_REG_SMPLRT_DIV      0x19    /**< Sample Rate = Gyro_rate / (1+SMPLRT_DIV) */
#define MPU6050_REG_CONFIG          0x1A    /**< DLPF config */
#define MPU6050_REG_GYRO_CONFIG     0x1B    /**< Full-scale range */
#define MPU6050_REG_ACCEL_CONFIG    0x1C
#define MPU6050_REG_INT_ENABLE      0x38
#define MPU6050_REG_ACCEL_XOUT_H    0x3B
#define MPU6050_REG_TEMP_OUT_H      0x41
#define MPU6050_REG_GYRO_XOUT_H     0x43
#define MPU6050_REG_PWR_MGMT_1      0x6B    /**< Power management, reset, clock source */
#define MPU6050_REG_PWR_MGMT_2      0x6C
#define MPU6050_REG_WHO_AM_I        0x75    /**< Returns 0x68 */

#define MPU6050_WHO_AM_I_VAL        0x68

// ── Accelerometer Full-Scale Range ──────────────────────────────────
typedef enum {
    MPU6050_ACCEL_FS_2G  = 0x00,   /**< ±2g,  LSB = 16384 /g */
    MPU6050_ACCEL_FS_4G  = 0x01,   /**< ±4g,  LSB = 8192  /g */
    MPU6050_ACCEL_FS_8G  = 0x02,   /**< ±8g,  LSB = 4096  /g */
    MPU6050_ACCEL_FS_16G = 0x03,   /**< ±16g, LSB = 2048  /g */
} mpu6050_accel_fs_t;

// ── Gyroscope Full-Scale Range ──────────────────────────────────────
typedef enum {
    MPU6050_GYRO_FS_250DPS  = 0x00,    /**< ±250 °/s,  LSB = 131   LSB/°/s */
    MPU6050_GYRO_FS_500DPS  = 0x01,    /**< ±500 °/s,  LSB = 65.5  LSB/°/s */
    MPU6050_GYRO_FS_1000DPS = 0x02,    /**< ±1000 °/s, LSB = 32.8  LSB/°/s */
    MPU6050_GYRO_FS_2000DPS = 0x03,    /**< ±2000 °/s, LSB = 16.4  LSB/°/s */
} mpu6050_gyro_fs_t;

// ── Digital Low-Pass Filter ─────────────────────────────────────────
// Higher DLPF = more filtering = lower bandwidth.
// DLPF_2 gives 94 Hz accel / 98 Hz gyro — good for gesture detection.
typedef enum {
    MPU6050_DLPF_260HZ = 0x00,
    MPU6050_DLPF_184HZ = 0x01,
    MPU6050_DLPF_94HZ  = 0x02,
    MPU6050_DLPF_44HZ  = 0x03,
    MPU6050_DLPF_21HZ  = 0x04,
    MPU6050_DLPF_10HZ  = 0x05,
    MPU6050_DLPF_5HZ   = 0x06,
} mpu6050_dlpf_t;

// ── Raw IMU Data ────────────────────────────────────────────────────
typedef struct {
    float    ax_g;          /**< Acceleration X (g) */
    float    ay_g;          /**< Acceleration Y (g) */
    float    az_g;          /**< Acceleration Z (g) */
    float    gx_dps;        /**< Angular rate X (°/s) */
    float    gy_dps;        /**< Angular rate Y (°/s) */
    float    gz_dps;        /**< Angular rate Z (°/s) */
    float    temp_c;        /**< Die temperature (°C) */
    uint32_t timestamp_ms;  /**< Millisecond timestamp */
    bool     valid;
} mpu6050_data_t;

// ── Device Configuration ────────────────────────────────────────────
typedef struct {
    const nrfx_twim_t  *twim;
    uint8_t             i2c_addr;
    mpu6050_accel_fs_t  accel_fs;
    mpu6050_gyro_fs_t   gyro_fs;
    mpu6050_dlpf_t      dlpf;
    uint8_t             sample_rate_div;    /**< 0 = max (1 kHz / (1+0) = 1 kHz) */
} mpu6050_config_t;

// ── Device Handle ───────────────────────────────────────────────────
typedef struct {
    mpu6050_config_t config;
    float  accel_lsb_per_g;
    float  gyro_lsb_per_dps;
    bool   initialised;
} mpu6050_dev_t;

// ── Gesture Types ───────────────────────────────────────────────────
typedef enum {
    GESTURE_NONE        = 0,
    GESTURE_ALIGN       = 1,    /**< Device held level and steady */
    GESTURE_SINGLE_WAVE = 2,    /**< Quick lateral wave */
    GESTURE_WRIST_FLICK = 3,    /**< Sharp wrist rotation */
} zemi_gesture_t;

// ── Gesture Detector State ───────────────────────────────────────────
// Internal state machine run by mpu6050_detect_gesture().
// Caller must maintain one instance per detection session.
typedef struct {
    // ALIGN state
    uint32_t align_stable_ms;       /**< Consecutive ms device has been level */

    // SINGLE_WAVE state
    float    wave_peak_g;           /**< Peak lateral accel seen this cycle */
    uint32_t wave_start_ms;
    bool     wave_in_progress;

    // WRIST_FLICK state
    float    flick_peak_dps;
    uint32_t flick_start_ms;
    bool     flick_in_progress;

    // Last detected gesture (cleared when caller reads it)
    zemi_gesture_t pending_gesture;
    uint32_t       gesture_timestamp_ms;
} gesture_detector_t;

// ── Gesture Thresholds ───────────────────────────────────────────────
// Tuned against Gen Alpha FY25 wrist movement data.
// Documented here so they appear in diffs when re-tuned.
#define GESTURE_ALIGN_GRAVITY_TOL_G     0.15f   /**< ±0.15g from 1g target */
#define GESTURE_ALIGN_GYRO_TOL_DPS      8.0f    /**< Wrist rotation still threshold */
#define GESTURE_ALIGN_STABLE_MS         300     /**< Must hold alignment 300 ms */

#define GESTURE_WAVE_ACCEL_THRESHOLD_G  0.6f    /**< Lateral peak to register a wave */
#define GESTURE_WAVE_RETURN_THRESHOLD_G 0.2f    /**< Below this = wave completed */
#define GESTURE_WAVE_MAX_DURATION_MS    600     /**< Too slow = not a wave */

#define GESTURE_FLICK_GYRO_THRESHOLD_DPS 80.0f  /**< Peak rotation rate for flick */
#define GESTURE_FLICK_MAX_DURATION_MS    300     /**< Must be brief */

// ── Default Configuration ───────────────────────────────────────────
// 100 Hz output rate: SMPLRT_DIV=9 → 1000/(1+9) = 100 Hz
// DLPF_94HZ keeps gesture bandwidth while rejecting >94 Hz noise
#define MPU6050_DEFAULT_CONFIG(_twim_instance_ptr) {    \
    .twim            = (_twim_instance_ptr),             \
    .i2c_addr        = MPU6050_I2C_ADDR_LOW,             \
    .accel_fs        = MPU6050_ACCEL_FS_4G,              \
    .gyro_fs         = MPU6050_GYRO_FS_500DPS,           \
    .dlpf            = MPU6050_DLPF_94HZ,                \
    .sample_rate_div = 9,                                \
}

// ── Public API ───────────────────────────────────────────────────────

/**
 * @brief Initialise the MPU-6050 and verify WHO_AM_I.
 * @return NRF_SUCCESS, or NRF_ERROR_NOT_FOUND if sensor absent.
 */
ret_code_t mpu6050_init(mpu6050_dev_t *dev, const mpu6050_config_t *config);

/**
 * @brief Soft-reset the MPU-6050 via PWR_MGMT_1.
 */
ret_code_t mpu6050_reset(mpu6050_dev_t *dev);

/**
 * @brief Read accelerometer + gyroscope + temperature in one burst.
 *        Call at the configured sample rate (default 100 Hz).
 */
ret_code_t mpu6050_read(mpu6050_dev_t *dev, mpu6050_data_t *out);

/**
 * @brief Initialise a gesture detector (zero all state).
 */
void gesture_detector_init(gesture_detector_t *det);

/**
 * @brief Feed one IMU sample into the gesture detector.
 *        Call at 100 Hz. Returns the gesture if one completed this sample,
 *        else GESTURE_NONE.
 * @param timestamp_ms  Current millisecond counter.
 */
zemi_gesture_t mpu6050_detect_gesture(gesture_detector_t *det,
                                       const mpu6050_data_t *data,
                                       uint32_t timestamp_ms);

/**
 * @brief Return human-readable gesture name.
 */
const char *gesture_name(zemi_gesture_t g);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_H */
