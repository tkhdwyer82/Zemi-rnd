/*
 * noise_filter_v32.c — D+V3.2 Dynamic Mode Firmware
 * CA1 Phase 3 — Firmware Iteration
 * Date: 2026-05-12 | Tag: ca1-p3-dynamic-play-02
 * Personnel: Timothy Dwyer — TKD Research and Consulting
 * ATO R&D Tax Incentive — Section 355 ITAA 1997 — ZEM-01
 *
 * Changes from D+V3.1:
 *   - Dynamic mode dwell window: 150ms -> 100ms
 *   - Motion flag trigger: angular velocity >15 deg/s
 *   - Static mode: D+V3.1 behaviour preserved
 */

#include "noise_filter.h"
#include "pairing_state_machine.h"

#define SPIKE_GATE_THRESHOLD_UT   50.0f
#define KALMAN_Q                  0.04f
#define KALMAN_R                  0.50f
#define ZLOCK_THRESHOLD_PCT       0.68f
#define DWELL_STATIC_MS           150
#define DWELL_DYNAMIC_MS          100
#define MOTION_FLAG_DEG_S         15.0f

typedef struct {
    float x, y, z;
} MagField;

typedef enum {
    MODE_STATIC,
    MODE_DYNAMIC
} DeviceMode;

static DeviceMode current_mode = MODE_STATIC;
static float kalman_state = 0.0f;
static float kalman_cov   = 1.0f;

static float kalman_update(float measurement) {
    kalman_cov += KALMAN_Q;
    float gain = kalman_cov / (kalman_cov + KALMAN_R);
    kalman_state += gain * (measurement - kalman_state);
    kalman_cov   *= (1.0f - gain);
    return kalman_state;
}

static bool spike_gate(float magnitude_ut) {
    return magnitude_ut <= SPIKE_GATE_THRESHOLD_UT;
}

static bool zlock_check(MagField *f) {
    float total = sqrtf(f->x*f->x + f->y*f->y + f->z*f->z);
    if (total == 0.0f) return false;
    return (fabsf(f->z) / total) >= ZLOCK_THRESHOLD_PCT;
}

void update_motion_flag(float angular_velocity_deg_s) {
    current_mode = (angular_velocity_deg_s > MOTION_FLAG_DEG_S)
                   ? MODE_DYNAMIC : MODE_STATIC;
}

uint32_t get_dwell_window_ms(void) {
    return (current_mode == MODE_DYNAMIC)
           ? DWELL_DYNAMIC_MS : DWELL_STATIC_MS;
}

bool process_sensor_reading(MagField *raw) {
    float mag = sqrtf(raw->x*raw->x + raw->y*raw->y + raw->z*raw->z);
    if (!spike_gate(mag)) return false;
    float filtered = kalman_update(mag);
    MagField filtered_field = {
        kalman_update(raw->x),
        kalman_update(raw->y),
        kalman_update(raw->z)
    };
    if (!zlock_check(&filtered_field)) return false;
    return true;
}
