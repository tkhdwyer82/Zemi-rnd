/**
 * noise_filter.c — Noise Filter Iterations A, B, C, D
 * Zemi R&D 2026 — Core Activity 1, Experiment 1.1
 *
 * RESEARCH QUESTION:
 *   Can a custom firmware noise-filtering algorithm reduce false-positive
 *   pairing events to <2% in a 10-device concurrent real-world test?
 *
 * HYPOTHESIS B (CA1):
 *   A hybrid spike-gate + Kalman filter (Iteration D) will reduce false-positive
 *   pairing events to <2% in a 10-device concurrent test environment, compared
 *   to the FY25 unfiltered baseline.
 *
 * EXPERIMENT DESIGN — 4 ITERATIONS:
 *   Iteration A — Baseline (no filter):   FY25 firmware, unfiltered raw µT readings.
 *   Iteration B — Moving Average:         8-sample sliding window per axis.
 *   Iteration C — Kalman Filter:          1D Kalman per axis, Gen Alpha-tuned Q/R.
 *   Iteration D — Hybrid (Spike+Kalman):  Spike gate (>50µT delta) pre-filtering
 *                                          then Kalman on gated signal.
 *
 * VARIABLES:
 *   Primary:   Filter algorithm type (categorical: A/B/C/D)
 *   Secondary: Window size (B), Q process noise / R measurement noise (C/D),
 *              Spike gate threshold in µT (D)
 *   Measured:  False-positive rate (%), pairing success rate (%), RMS noise (µT)
 *
 * SUCCESS CRITERION: False-positive rate <2% under 10-device concurrent test
 *
 * ATO R&D Note: These four implementations embody genuine experimentation —
 * the optimal algorithm for Gen Alpha wrist-motion + multi-device EMI environment
 * could not be determined in advance by a competent professional.
 *
 * ATO Substantiation: docs/experiment_log.md — Experiment 1.1
 * GitHub: tkhdwyer82/Zemi-rnd
 * Path:   firmware/core_activity_1_magnetic_sensor/src/noise_filter.c
 */

#include "noise_filter.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "NOISE_FILTER";

/* -----------------------------------------------------------------------
 * ITERATION A — Baseline (No Filter)
 * FY25 firmware behaviour: raw sensor reading passes through unchanged.
 * Establishes the false-positive and noise baseline against which B, C, D
 * are compared.
 * ----------------------------------------------------------------------- */
void noise_filter_iteration_a(noise_filter_t *f, mlx90393_data_t *in, mlx90393_data_t *out)
{
    (void)f;  /* No state used in baseline */
    *out = *in;
    ESP_LOGD(TAG, "[A-Baseline] x=%.2f y=%.2f z=%.2f |B|=%.2f µT",
             out->x_ut, out->y_ut, out->z_ut, out->magnitude_ut);
}

/* -----------------------------------------------------------------------
 * ITERATION B — Moving Average (8-sample window, per axis)
 *
 * Variable under test: window_size (default 8, tunable 4–16)
 * Rationale: Simple low-pass filter. Expected to reduce high-frequency
 * environmental noise but may not handle impulsive EMI spikes from nearby
 * devices (laptops, classroom electronics). Intermediate hypothesis: this
 * will improve false-positive rate vs baseline but not reach <2% target
 * in a dense multi-device environment.
 * ----------------------------------------------------------------------- */
void noise_filter_init_b(noise_filter_t *f, uint8_t window_size)
{
    memset(f, 0, sizeof(noise_filter_t));
    f->type = FILTER_ITERATION_B;
    f->b.window_size = (window_size > 0 && window_size <= MOVING_AVG_MAX_WINDOW)
                        ? window_size : 8;
    f->b.head = 0;
    f->b.count = 0;
    ESP_LOGI(TAG, "[B-MovAvg] Initialised window_size=%u", f->b.window_size);
}

void noise_filter_iteration_b(noise_filter_t *f, mlx90393_data_t *in, mlx90393_data_t *out)
{
    moving_avg_state_t *s = &f->b;

    /* Insert new sample at head position */
    s->buf_x[s->head] = in->x_ut;
    s->buf_y[s->head] = in->y_ut;
    s->buf_z[s->head] = in->z_ut;
    s->head = (s->head + 1) % s->window_size;
    if (s->count < s->window_size) s->count++;

    /* Compute average over filled portion of window */
    float sum_x = 0, sum_y = 0, sum_z = 0;
    for (uint8_t i = 0; i < s->count; i++) {
        sum_x += s->buf_x[i];
        sum_y += s->buf_y[i];
        sum_z += s->buf_z[i];
    }
    out->x_ut = sum_x / s->count;
    out->y_ut = sum_y / s->count;
    out->z_ut = sum_z / s->count;
    out->magnitude_ut = sqrtf(out->x_ut * out->x_ut +
                               out->y_ut * out->y_ut +
                               out->z_ut * out->z_ut);

    ESP_LOGD(TAG, "[B-MovAvg] x=%.2f y=%.2f z=%.2f |B|=%.2f µT (n=%u)",
             out->x_ut, out->y_ut, out->z_ut, out->magnitude_ut, s->count);
}

/* -----------------------------------------------------------------------
 * ITERATION C — 1D Kalman Filter (per axis), Gen Alpha-tuned Q/R
 *
 * Variables under test:
 *   Q = process noise covariance (models wrist motion dynamics)
 *   R = measurement noise covariance (models sensor + EMI noise)
 *
 * Gen Alpha tuning rationale: Adult Kalman parameters from literature
 * (Q≈0.01, R≈0.1) calibrated for slow deliberate motion. Gen Alpha
 * motor patterns are faster, more impulsive (TikTok/YouTube swipe-trained).
 * Initial Gen Alpha values: Q=0.05, R=0.5 — higher process noise to
 * track fast wrist motion; higher R to suppress EMI artefacts.
 * These are experimental starting points; Experiment 1.1 Phase 2 will
 * iterate Q and R values and compare false-positive rates.
 * ----------------------------------------------------------------------- */
void noise_filter_init_c(noise_filter_t *f, float Q, float R)
{
    memset(f, 0, sizeof(noise_filter_t));
    f->type = FILTER_ITERATION_C;
    /* Initialise Kalman state for each axis */
    for (int i = 0; i < 3; i++) {
        f->c.axis[i].Q = Q;
        f->c.axis[i].R = R;
        f->c.axis[i].P = 1.0f;   /* Initial estimate uncertainty */
        f->c.axis[i].x_est = 0.0f; /* Initial state estimate */
        f->c.axis[i].initialised = false;
    }
    ESP_LOGI(TAG, "[C-Kalman] Initialised Q=%.4f R=%.4f", Q, R);
}

static float kalman_update(kalman_axis_t *k, float measurement)
{
    /* First measurement: seed estimate directly to avoid transient */
    if (!k->initialised) {
        k->x_est = measurement;
        k->initialised = true;
        return measurement;
    }

    /* Prediction step: state model is constant (no motion model — wrist
     * orientation treated as slowly varying vs. measurement rate) */
    float P_pred = k->P + k->Q;

    /* Update step */
    float K = P_pred / (P_pred + k->R);  /* Kalman gain */
    k->x_est = k->x_est + K * (measurement - k->x_est);
    k->P = (1.0f - K) * P_pred;

    return k->x_est;
}

void noise_filter_iteration_c(noise_filter_t *f, mlx90393_data_t *in, mlx90393_data_t *out)
{
    out->x_ut = kalman_update(&f->c.axis[0], in->x_ut);
    out->y_ut = kalman_update(&f->c.axis[1], in->y_ut);
    out->z_ut = kalman_update(&f->c.axis[2], in->z_ut);
    out->magnitude_ut = sqrtf(out->x_ut * out->x_ut +
                               out->y_ut * out->y_ut +
                               out->z_ut * out->z_ut);

    ESP_LOGD(TAG, "[C-Kalman] x=%.2f y=%.2f z=%.2f |B|=%.2f µT",
             out->x_ut, out->y_ut, out->z_ut, out->magnitude_ut);
}

/* -----------------------------------------------------------------------
 * ITERATION D — Hybrid: Spike Gate (>50µT delta) + Kalman
 *
 * Variables under test:
 *   spike_gate_threshold_ut: delta-µT above which a sample is rejected as
 *                             an EMI spike rather than genuine wrist motion.
 *                             Default 50µT; range under test: 20–100µT.
 *   Q, R: Kalman parameters (inherited from best-performing C iteration).
 *
 * Rationale: In a 10-device classroom environment, devices transmitting
 * magnetic signals can cause impulsive spikes that overwhelm the Kalman
 * filter. The spike gate pre-rejects these samples before the Kalman
 * smoother runs, preserving the genuine wrist-gesture signal.
 * Hypothesis: spike rejection at 50µT threshold + Kalman will push
 * false-positive rate below <2% threshold where C alone could not.
 * ----------------------------------------------------------------------- */
void noise_filter_init_d(noise_filter_t *f, float gate_threshold_ut, float Q, float R)
{
    memset(f, 0, sizeof(noise_filter_t));
    f->type = FILTER_ITERATION_D;
    f->d.spike_threshold_ut = gate_threshold_ut;

    for (int i = 0; i < 3; i++) {
        f->d.kalman.axis[i].Q = Q;
        f->d.kalman.axis[i].R = R;
        f->d.kalman.axis[i].P = 1.0f;
        f->d.kalman.axis[i].x_est = 0.0f;
        f->d.kalman.axis[i].initialised = false;
        f->d.prev[i] = 0.0f;
    }
    f->d.spike_count = 0;
    ESP_LOGI(TAG, "[D-Hybrid] gate=%.1fµT Q=%.4f R=%.4f", gate_threshold_ut, Q, R);
}

void noise_filter_iteration_d(noise_filter_t *f, mlx90393_data_t *in, mlx90393_data_t *out)
{
    hybrid_state_t *s = &f->d;
    float vals[3] = { in->x_ut, in->y_ut, in->z_ut };
    float filtered[3];

    for (int i = 0; i < 3; i++) {
        float delta = fabsf(vals[i] - s->prev[i]);

        if (delta > s->spike_threshold_ut) {
            /* Spike detected — reject this sample, hold previous estimate */
            s->spike_count++;
            ESP_LOGW(TAG, "[D-Hybrid] Spike rejected axis[%d] delta=%.1fµT (total spikes=%lu)",
                     i, delta, (unsigned long)s->spike_count);
            filtered[i] = kalman_update(&s->kalman.axis[i], s->prev[i]); /* feed prev to Kalman */
        } else {
            /* Clean sample — pass through Kalman */
            s->prev[i] = vals[i];
            filtered[i] = kalman_update(&s->kalman.axis[i], vals[i]);
        }
    }

    out->x_ut = filtered[0];
    out->y_ut = filtered[1];
    out->z_ut = filtered[2];
    out->magnitude_ut = sqrtf(out->x_ut * out->x_ut +
                               out->y_ut * out->y_ut +
                               out->z_ut * out->z_ut);

    ESP_LOGD(TAG, "[D-Hybrid] x=%.2f y=%.2f z=%.2f |B|=%.2f µT spikes=%lu",
             out->x_ut, out->y_ut, out->z_ut, out->magnitude_ut, (unsigned long)s->spike_count);
}

/* -----------------------------------------------------------------------
 * Unified dispatch — select active filter iteration at runtime
 * (allows switching between A/B/C/D without reflashing for comparative testing)
 * ----------------------------------------------------------------------- */
void noise_filter_apply(noise_filter_t *f, mlx90393_data_t *in, mlx90393_data_t *out)
{
    switch (f->type) {
        case FILTER_ITERATION_A: noise_filter_iteration_a(f, in, out); break;
        case FILTER_ITERATION_B: noise_filter_iteration_b(f, in, out); break;
        case FILTER_ITERATION_C: noise_filter_iteration_c(f, in, out); break;
        case FILTER_ITERATION_D: noise_filter_iteration_d(f, in, out); break;
        default:
            ESP_LOGE(TAG, "Unknown filter type %d — using baseline", f->type);
            noise_filter_iteration_a(f, in, out);
    }
}

uint32_t noise_filter_get_spike_count(noise_filter_t *f)
{
    if (f->type == FILTER_ITERATION_D) return f->d.spike_count;
    return 0;
}
