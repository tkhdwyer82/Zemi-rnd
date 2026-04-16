/**
 * @file noise_filter.h
 * @brief Noise Filtering Algorithms for MLX90393 — Iteration A/B/C/D
 *
 * Zemi R&D 2026 — Core Activity 1, Experiment 1.1
 *
 * Research question:
 *   Which firmware-level noise-filtering approach minimises false-positive
 *   pairing events while maintaining >= 95% true detection under EMI?
 *
 * Iterations:
 *   A — Baseline (no filtering, from FY25 firmware)
 *   B — Moving Average Filter
 *   C — Kalman Filter (calibrated to Gen Alpha movement signatures)
 *   D — Hybrid: Threshold Gate + Kalman (optimised for multi-device)
 *
 * Contractor: Timothy Dwyer (TKD Research and Consulting)
 * Company:    Zemi Pty Ltd
 */

#ifndef NOISE_FILTER_H
#define NOISE_FILTER_H

#include <stdint.h>
#include <stdbool.h>
#include "mlx90393.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Filter Type Selector ────────────────────────────────────────────
typedef enum {
    ZEMI_FILTER_NONE        = 0,    /**< Iteration A: Baseline (FY25) */
    ZEMI_FILTER_MOVING_AVG  = 1,    /**< Iteration B: Moving Average */
    ZEMI_FILTER_KALMAN      = 2,    /**< Iteration C: Kalman */
    ZEMI_FILTER_HYBRID      = 3,    /**< Iteration D: Threshold + Kalman */
} zemi_filter_type_t;

// ── Moving Average ───────────────────────────────────────────────────
#define MOVING_AVG_WINDOW   8       /**< Tunable: increase for more smoothing */

typedef struct {
    float   buf_x[MOVING_AVG_WINDOW];
    float   buf_y[MOVING_AVG_WINDOW];
    float   buf_z[MOVING_AVG_WINDOW];
    uint8_t head;
    uint8_t count;
} moving_avg_state_t;

// ── Kalman Filter (1D per axis) ──────────────────────────────────────
// Process noise Q and measurement noise R are key tuning parameters.
// Q large = trusts measurement more (faster response, more noise)
// R large = trusts model more (smoother, slower response)
// These values are calibrated against Gen Alpha movement data from FY25.
#define KALMAN_Q_DEFAULT    0.01f   /**< Process noise — Gen Alpha tuned */
#define KALMAN_R_DEFAULT    0.5f    /**< Measurement noise — Gen Alpha tuned */

typedef struct {
    float estimate;     /**< Current state estimate */
    float error_est;    /**< Estimation error covariance */
    float q;            /**< Process noise covariance */
    float r;            /**< Measurement noise covariance */
} kalman1d_t;

typedef struct {
    kalman1d_t kx;
    kalman1d_t ky;
    kalman1d_t kz;
} kalman_state_t;

// ── Hybrid (Threshold Gate + Kalman) ────────────────────────────────
// The gate rejects samples where the magnitude change exceeds a threshold
// caused by interference spikes, before passing to the Kalman filter.
// Tuned for multi-device classroom environments.
#define HYBRID_SPIKE_GATE_uT    50.0f   /**< Reject spikes > 50uT delta */

typedef struct {
    kalman_state_t  kalman;
    float           prev_magnitude;
    uint32_t        spike_count;        /**< For R&D telemetry logging */
} hybrid_state_t;

// ── Unified Filter State ─────────────────────────────────────────────
typedef struct {
    zemi_filter_type_t  type;
    union {
        moving_avg_state_t  moving_avg;
        kalman_state_t      kalman;
        hybrid_state_t      hybrid;
    };
    // ── R&D telemetry (logged for ATO substantiation) ──────────────
    uint32_t    samples_processed;
    uint32_t    samples_rejected;
    float       last_magnitude_uT;
} zemi_filter_state_t;

// ── API ──────────────────────────────────────────────────────────────

/**
 * @brief Initialise filter state for selected iteration.
 */
void zemi_filter_init(zemi_filter_state_t *state, zemi_filter_type_t type);

/**
 * @brief Apply filter to a raw measurement. Returns filtered result.
 *        Sets result->valid = false if sample was rejected.
 */
void zemi_filter_apply(zemi_filter_state_t *state,
                        const mlx90393_measurement_t *raw,
                        mlx90393_measurement_t *result);

/**
 * @brief Reset filter state (e.g. between test runs).
 */
void zemi_filter_reset(zemi_filter_state_t *state);

/**
 * @brief Return name string for logging ("NONE","MOVING_AVG","KALMAN","HYBRID")
 */
const char *zemi_filter_name(zemi_filter_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NOISE_FILTER_H */
