/**
 * @file noise_filter.c
 * @brief Noise Filtering Algorithms — Implementation (Iterations A–D)
 *
 * Zemi R&D 2026 — Core Activity 1, Experiment 1.1
 * Contractor: Timothy Dwyer (TKD Research and Consulting)
 * Company:    Zemi Pty Ltd
 */

#include "noise_filter.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "ZEMI_FILTER";

// ─────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────

static void _kalman_init(kalman1d_t *k, float q, float r)
{
    k->estimate  = 0.0f;
    k->error_est = 1.0f;
    k->q = q;
    k->r = r;
}

static float _kalman_update(kalman1d_t *k, float measurement)
{
    // Prediction step
    float error_pred = k->error_est + k->q;

    // Update step (Kalman gain)
    float gain = error_pred / (error_pred + k->r);
    k->estimate  = k->estimate + gain * (measurement - k->estimate);
    k->error_est = (1.0f - gain) * error_pred;

    return k->estimate;
}

static float _moving_avg_update(float *buf, uint8_t *head, uint8_t *count,
                                  uint8_t window, float new_val)
{
    buf[*head] = new_val;
    *head = (*head + 1) % window;
    if (*count < window) (*count)++;

    float sum = 0.0f;
    for (uint8_t i = 0; i < *count; i++) {
        sum += buf[i];
    }
    return sum / *count;
}

// ─────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────

void zemi_filter_init(zemi_filter_state_t *state, zemi_filter_type_t type)
{
    memset(state, 0, sizeof(*state));
    state->type = type;

    switch (type) {
        case ZEMI_FILTER_KALMAN:
            _kalman_init(&state->kalman.kx, KALMAN_Q_DEFAULT, KALMAN_R_DEFAULT);
            _kalman_init(&state->kalman.ky, KALMAN_Q_DEFAULT, KALMAN_R_DEFAULT);
            _kalman_init(&state->kalman.kz, KALMAN_Q_DEFAULT, KALMAN_R_DEFAULT);
            break;

        case ZEMI_FILTER_HYBRID:
            _kalman_init(&state->hybrid.kalman.kx, KALMAN_Q_DEFAULT, KALMAN_R_DEFAULT);
            _kalman_init(&state->hybrid.kalman.ky, KALMAN_Q_DEFAULT, KALMAN_R_DEFAULT);
            _kalman_init(&state->hybrid.kalman.kz, KALMAN_Q_DEFAULT, KALMAN_R_DEFAULT);
            state->hybrid.prev_magnitude = 0.0f;
            state->hybrid.spike_count    = 0;
            break;

        case ZEMI_FILTER_MOVING_AVG:
        case ZEMI_FILTER_NONE:
        default:
            break;
    }

    ESP_LOGI(TAG, "Filter initialised: %s", zemi_filter_name(type));
}

void zemi_filter_apply(zemi_filter_state_t *state,
                        const mlx90393_measurement_t *raw,
                        mlx90393_measurement_t *result)
{
    *result = *raw; // copy timestamp, temp, valid flag
    state->samples_processed++;

    float mag = mlx90393_magnitude(raw);
    state->last_magnitude_uT = mag;

    switch (state->type) {

        // ── Iteration A: No filtering (FY25 baseline) ───────────────
        case ZEMI_FILTER_NONE:
            result->x_uT = raw->x_uT;
            result->y_uT = raw->y_uT;
            result->z_uT = raw->z_uT;
            break;

        // ── Iteration B: Moving Average ─────────────────────────────
        case ZEMI_FILTER_MOVING_AVG: {
            moving_avg_state_t *s = &state->moving_avg;
            result->x_uT = _moving_avg_update(
                s->buf_x, &s->head, &s->count, MOVING_AVG_WINDOW, raw->x_uT);
            result->y_uT = _moving_avg_update(
                s->buf_y, &s->head, &s->count, MOVING_AVG_WINDOW, raw->y_uT);
            result->z_uT = _moving_avg_update(
                s->buf_z, &s->head, &s->count, MOVING_AVG_WINDOW, raw->z_uT);
            break;
        }

        // ── Iteration C: Kalman Filter ──────────────────────────────
        case ZEMI_FILTER_KALMAN: {
            kalman_state_t *k = &state->kalman;
            result->x_uT = _kalman_update(&k->kx, raw->x_uT);
            result->y_uT = _kalman_update(&k->ky, raw->y_uT);
            result->z_uT = _kalman_update(&k->kz, raw->z_uT);
            break;
        }

        // ── Iteration D: Hybrid (Spike Gate + Kalman) ───────────────
        case ZEMI_FILTER_HYBRID: {
            hybrid_state_t *h = &state->hybrid;
            float delta = fabsf(mag - h->prev_magnitude);

            if (h->prev_magnitude > 0.0f && delta > HYBRID_SPIKE_GATE_uT) {
                // Spike detected — reject this sample
                result->valid = false;
                h->spike_count++;
                state->samples_rejected++;
                ESP_LOGD(TAG, "Spike rejected: delta=%.1f uT (total=%lu)",
                         delta, (unsigned long)h->spike_count);
                // Return the last Kalman estimate unchanged
                result->x_uT = h->kalman.kx.estimate;
                result->y_uT = h->kalman.ky.estimate;
                result->z_uT = h->kalman.kz.estimate;
            } else {
                // Pass through Kalman
                result->x_uT = _kalman_update(&h->kalman.kx, raw->x_uT);
                result->y_uT = _kalman_update(&h->kalman.ky, raw->y_uT);
                result->z_uT = _kalman_update(&h->kalman.kz, raw->z_uT);
                result->valid = true;
            }
            h->prev_magnitude = mag;
            break;
        }
    }
}

void zemi_filter_reset(zemi_filter_state_t *state)
{
    zemi_filter_type_t type = state->type;
    zemi_filter_init(state, type);
}

const char *zemi_filter_name(zemi_filter_type_t type)
{
    switch (type) {
        case ZEMI_FILTER_NONE:       return "NONE (Baseline A)";
        case ZEMI_FILTER_MOVING_AVG: return "MOVING_AVG (B)";
        case ZEMI_FILTER_KALMAN:     return "KALMAN (C)";
        case ZEMI_FILTER_HYBRID:     return "HYBRID Threshold+Kalman (D)";
        default:                     return "UNKNOWN";
    }
}
