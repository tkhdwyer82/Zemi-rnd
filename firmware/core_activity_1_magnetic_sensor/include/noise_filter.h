/**
 * noise_filter.h — Noise Filter Iterations A–D Header
 * Zemi R&D 2026 — Core Activity 1, Experiment 1.1
 */

#pragma once
#include "mlx90393.h"
#include <stdint.h>
#include <stdbool.h>

#define MOVING_AVG_MAX_WINDOW  16

typedef enum {
    FILTER_ITERATION_A = 0,  /**< Baseline — no filter (FY25 behaviour) */
    FILTER_ITERATION_B,      /**< Moving average, 8-sample window        */
    FILTER_ITERATION_C,      /**< 1D Kalman per axis, Gen Alpha Q/R      */
    FILTER_ITERATION_D,      /**< Hybrid: spike gate + Kalman            */
} filter_type_t;

typedef struct {
    float Q;           /**< Process noise covariance */
    float R;           /**< Measurement noise covariance */
    float P;           /**< Estimate uncertainty */
    float x_est;       /**< Current state estimate */
    bool  initialised;
} kalman_axis_t;

typedef struct {
    kalman_axis_t axis[3];  /**< [0]=X, [1]=Y, [2]=Z */
} kalman_state_t;

typedef struct {
    float    buf_x[MOVING_AVG_MAX_WINDOW];
    float    buf_y[MOVING_AVG_MAX_WINDOW];
    float    buf_z[MOVING_AVG_MAX_WINDOW];
    uint8_t  head;
    uint8_t  count;
    uint8_t  window_size;
} moving_avg_state_t;

typedef struct {
    float          spike_threshold_ut;  /**< µT delta above which sample is rejected */
    kalman_state_t kalman;
    float          prev[3];             /**< Previous accepted sample per axis */
    uint32_t       spike_count;         /**< Total spikes rejected (for logging) */
} hybrid_state_t;

typedef struct {
    filter_type_t type;
    union {
        moving_avg_state_t b;
        kalman_state_t     c;
        hybrid_state_t     d;
    };
} noise_filter_t;

/** Iteration A: pass-through baseline */
void noise_filter_iteration_a(noise_filter_t *f, mlx90393_data_t *in, mlx90393_data_t *out);

/** Iteration B: initialise moving average filter */
void noise_filter_init_b(noise_filter_t *f, uint8_t window_size);
void noise_filter_iteration_b(noise_filter_t *f, mlx90393_data_t *in, mlx90393_data_t *out);

/** Iteration C: initialise Kalman filter with explicit Q, R */
void noise_filter_init_c(noise_filter_t *f, float Q, float R);
void noise_filter_iteration_c(noise_filter_t *f, mlx90393_data_t *in, mlx90393_data_t *out);

/** Iteration D: initialise hybrid spike-gate + Kalman */
void noise_filter_init_d(noise_filter_t *f, float gate_threshold_ut, float Q, float R);
void noise_filter_iteration_d(noise_filter_t *f, mlx90393_data_t *in, mlx90393_data_t *out);

/** Unified dispatch — routes to active iteration */
void noise_filter_apply(noise_filter_t *f, mlx90393_data_t *in, mlx90393_data_t *out);

/** Returns spike count for Iteration D logging (0 for A/B/C) */
uint32_t noise_filter_get_spike_count(noise_filter_t *f);
