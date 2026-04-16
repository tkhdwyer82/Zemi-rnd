/**
 * @file pairing_state_machine.c
 * @brief Magnetic Pairing State Machine — Implementation (V1/V2/V3)
 *
 * Zemi R&D 2026 — Core Activity 1, Experiment 1.2
 * Contractor: Timothy Dwyer (TKD Research and Consulting)
 * Company:    Zemi Pty Ltd
 */

#include "pairing_state_machine.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>
#include <string.h>

static const char *TAG = "ZEMI_PSM";

// ── Internal ──────────────────────────────────────────────────────────

static inline int64_t _now_us(void) { return esp_timer_get_time(); }
static inline int64_t _now_ms(void) { return esp_timer_get_time() / 1000; }

static void _enter_state(psm_handle_t *h, pair_state_t new_state)
{
    ESP_LOGI(TAG, "[%s] %s → %s",
             psm_variant_name(h->variant),
             psm_state_name(h->state),
             psm_state_name(new_state));
    h->state = new_state;
    h->state_entry_time_us = _now_us();
}

static void _record_pair_latency(psm_handle_t *h, float latency_ms)
{
    psm_telemetry_t *t = &h->telemetry;
    t->successful_pairs++;

    // Running average
    uint32_t n = t->successful_pairs;
    t->avg_pair_latency_ms =
        (t->avg_pair_latency_ms * (n - 1) + latency_ms) / n;

    if (latency_ms < t->min_pair_latency_ms || t->min_pair_latency_ms == 0.0f)
        t->min_pair_latency_ms = latency_ms;
    if (latency_ms > t->max_pair_latency_ms)
        t->max_pair_latency_ms = latency_ms;

    ESP_LOGI(TAG, "Pair latency: %.1f ms (avg=%.1f, min=%.1f, max=%.1f)",
             latency_ms,
             t->avg_pair_latency_ms,
             t->min_pair_latency_ms,
             t->max_pair_latency_ms);
}

// ── V1: Simple Threshold ──────────────────────────────────────────────
static pair_event_t _update_v1(psm_handle_t *h, float mag)
{
    switch (h->state) {
        case PAIR_STATE_SCANNING:
            if (mag >= PAIR_THRESHOLD_uT && mag <= PAIR_MAX_DISTANCE_uT) {
                float latency_ms = (float)(_now_us() - h->state_entry_time_us) / 1000.0f;
                _record_pair_latency(h, latency_ms);
                _enter_state(h, PAIR_STATE_PAIRED);
                return PAIR_EVENT_PAIRED;
            }
            // Timeout check
            if ((_now_ms() - h->state_entry_time_us / 1000) > PAIR_TIMEOUT_MS) {
                h->telemetry.timeouts++;
                _enter_state(h, PAIR_STATE_ERROR);
                return PAIR_EVENT_TIMEOUT;
            }
            break;

        case PAIR_STATE_PAIRED:
            if (mag < PAIR_THRESHOLD_uT) {
                _enter_state(h, PAIR_STATE_RECONNECTING);
                return PAIR_EVENT_UNPAIRED;
            }
            break;

        case PAIR_STATE_RECONNECTING:
            if (mag >= PAIR_THRESHOLD_uT) {
                h->telemetry.reconnect_successes++;
                _enter_state(h, PAIR_STATE_PAIRED);
                return PAIR_EVENT_RECONNECT_OK;
            }
            if ((_now_ms() - h->state_entry_time_us / 1000) > PAIR_RECONNECT_TIMEOUT_MS) {
                _enter_state(h, PAIR_STATE_SCANNING);
            }
            break;

        default: break;
    }
    return PAIR_EVENT_NONE;
}

// ── V2: Dwell + Confirm ───────────────────────────────────────────────
static pair_event_t _update_v2(psm_handle_t *h, float mag)
{
    int64_t now_ms = _now_ms();

    switch (h->state) {
        case PAIR_STATE_SCANNING:
            if (mag >= PAIR_THRESHOLD_uT && mag <= PAIR_MAX_DISTANCE_uT) {
                // Field first detected — move to DETECTING
                h->dwell_start_us  = _now_us();
                h->dwell_peak_uT   = mag;
                _enter_state(h, PAIR_STATE_DETECTING);
                return PAIR_EVENT_FIELD_DETECTED;
            }
            if ((now_ms - h->state_entry_time_us / 1000) > PAIR_TIMEOUT_MS) {
                h->telemetry.timeouts++;
                _enter_state(h, PAIR_STATE_ERROR);
                return PAIR_EVENT_TIMEOUT;
            }
            break;

        case PAIR_STATE_DETECTING:
            if (mag < PAIR_THRESHOLD_uT) {
                // Field dropped — false positive, back to scanning
                h->telemetry.false_positives++;
                ESP_LOGW(TAG, "V2: Field lost during dwell — false positive");
                _enter_state(h, PAIR_STATE_SCANNING);
                break;
            }
            if (mag > h->dwell_peak_uT) h->dwell_peak_uT = mag;

            // Check dwell time
            int64_t dwell_ms = (_now_us() - h->dwell_start_us) / 1000;
            if (dwell_ms >= PAIR_DWELL_MS) {
                float latency_ms = (float)(_now_us() - h->state_entry_time_us) / 1000.0f
                                 + (float)PAIR_DWELL_MS;
                _record_pair_latency(h, latency_ms);
                _enter_state(h, PAIR_STATE_PAIRED);
                return PAIR_EVENT_PAIRED;
            }
            break;

        case PAIR_STATE_PAIRED:
            if (mag < PAIR_THRESHOLD_uT) {
                _enter_state(h, PAIR_STATE_RECONNECTING);
                return PAIR_EVENT_UNPAIRED;
            }
            break;

        case PAIR_STATE_RECONNECTING:
            if (mag >= PAIR_THRESHOLD_uT) {
                h->telemetry.reconnect_successes++;
                _enter_state(h, PAIR_STATE_PAIRED);
                return PAIR_EVENT_RECONNECT_OK;
            }
            if ((now_ms - h->state_entry_time_us / 1000) > PAIR_RECONNECT_TIMEOUT_MS) {
                _enter_state(h, PAIR_STATE_SCANNING);
            }
            break;

        default: break;
    }
    return PAIR_EVENT_NONE;
}

// ── V3: Directional (Z-axis dominant) ────────────────────────────────
// Only pairs when the approaching device's field is predominantly in the
// Z-axis, meaning it is being held face-to-face (natural pairing gesture).
// This reduces false pairings from nearby devices on the same wrist plane.
static pair_event_t _update_v3(psm_handle_t *h, float mag)
{
    if (mag < 0.001f) return PAIR_EVENT_NONE; // guard div-by-zero

    float z_ratio = fabsf(h->last_measurement.z_uT) / mag;
    bool z_dominant = (z_ratio >= PAIR_Z_DOMINANCE_RATIO);

    int64_t now_ms = _now_ms();

    switch (h->state) {
        case PAIR_STATE_SCANNING:
            if (mag >= PAIR_THRESHOLD_uT && mag <= PAIR_MAX_DISTANCE_uT && z_dominant) {
                h->dwell_start_us = _now_us();
                h->dwell_peak_uT  = mag;
                _enter_state(h, PAIR_STATE_DETECTING);
                return PAIR_EVENT_FIELD_DETECTED;
            }
            if ((now_ms - h->state_entry_time_us / 1000) > PAIR_TIMEOUT_MS) {
                h->telemetry.timeouts++;
                _enter_state(h, PAIR_STATE_ERROR);
                return PAIR_EVENT_TIMEOUT;
            }
            break;

        case PAIR_STATE_DETECTING:
            if (mag < PAIR_THRESHOLD_uT || !z_dominant) {
                h->telemetry.false_positives++;
                ESP_LOGW(TAG, "V3: Z-dominance lost (z_ratio=%.2f) — back to scan", z_ratio);
                _enter_state(h, PAIR_STATE_SCANNING);
                break;
            }
            {
                int64_t dwell_ms = (_now_us() - h->dwell_start_us) / 1000;
                if (dwell_ms >= PAIR_DWELL_MS) {
                    float latency_ms = (float)(_now_us() - h->state_entry_time_us) / 1000.0f;
                    _record_pair_latency(h, latency_ms);
                    _enter_state(h, PAIR_STATE_PAIRED);
                    return PAIR_EVENT_PAIRED;
                }
            }
            break;

        case PAIR_STATE_PAIRED:
            if (mag < PAIR_THRESHOLD_uT) {
                _enter_state(h, PAIR_STATE_RECONNECTING);
                return PAIR_EVENT_UNPAIRED;
            }
            break;

        case PAIR_STATE_RECONNECTING:
            if (mag >= PAIR_THRESHOLD_uT) {
                h->telemetry.reconnect_successes++;
                _enter_state(h, PAIR_STATE_PAIRED);
                return PAIR_EVENT_RECONNECT_OK;
            }
            if ((now_ms - h->state_entry_time_us / 1000) > PAIR_RECONNECT_TIMEOUT_MS) {
                _enter_state(h, PAIR_STATE_SCANNING);
            }
            break;

        default: break;
    }
    return PAIR_EVENT_NONE;
}

// ── Public API ────────────────────────────────────────────────────────

void psm_init(psm_handle_t *handle,
              psm_variant_t variant,
              zemi_filter_type_t filter_type,
              psm_event_cb_t cb,
              void *user_data)
{
    memset(handle, 0, sizeof(*handle));
    handle->variant = variant;
    handle->state   = PAIR_STATE_IDLE;
    zemi_filter_init(&handle->filter, filter_type);

    ESP_LOGI(TAG, "PSM initialised: variant=%s, filter=%s",
             psm_variant_name(variant),
             zemi_filter_name(filter_type));
}

void psm_start_scanning(psm_handle_t *handle)
{
    handle->telemetry.total_pair_attempts++;
    _enter_state(handle, PAIR_STATE_SCANNING);
}

pair_event_t psm_update(psm_handle_t *handle,
                         const mlx90393_measurement_t *raw,
                         psm_event_cb_t cb, void *user_data)
{
    // Apply noise filter
    mlx90393_measurement_t filtered;
    zemi_filter_apply(&handle->filter, raw, &filtered);
    handle->last_measurement = filtered;

    if (!filtered.valid) return PAIR_EVENT_NONE;

    float mag = mlx90393_magnitude(&filtered);
    handle->last_magnitude_uT = mag;

    pair_event_t event = PAIR_EVENT_NONE;

    switch (handle->variant) {
        case PSM_VARIANT_V1_SIMPLE:      event = _update_v1(handle, mag); break;
        case PSM_VARIANT_V2_DWELL:       event = _update_v2(handle, mag); break;
        case PSM_VARIANT_V3_DIRECTIONAL: event = _update_v3(handle, mag); break;
    }

    if (event != PAIR_EVENT_NONE && cb) {
        cb(event, handle, user_data);
    }
    return event;
}

void psm_unpair(psm_handle_t *handle)
{
    _enter_state(handle, PAIR_STATE_IDLE);
}

pair_state_t psm_get_state(const psm_handle_t *handle)
{
    return handle->state;
}

psm_telemetry_t psm_get_telemetry(const psm_handle_t *handle)
{
    return handle->telemetry;
}

void psm_reset_telemetry(psm_handle_t *handle)
{
    memset(&handle->telemetry, 0, sizeof(handle->telemetry));
}

const char *psm_state_name(pair_state_t state)
{
    switch (state) {
        case PAIR_STATE_IDLE:         return "IDLE";
        case PAIR_STATE_SCANNING:     return "SCANNING";
        case PAIR_STATE_DETECTING:    return "DETECTING";
        case PAIR_STATE_PAIRED:       return "PAIRED";
        case PAIR_STATE_RECONNECTING: return "RECONNECTING";
        case PAIR_STATE_ERROR:        return "ERROR";
        default:                      return "UNKNOWN";
    }
}

const char *psm_event_name(pair_event_t event)
{
    switch (event) {
        case PAIR_EVENT_NONE:             return "NONE";
        case PAIR_EVENT_SCANNING_STARTED: return "SCANNING_STARTED";
        case PAIR_EVENT_FIELD_DETECTED:   return "FIELD_DETECTED";
        case PAIR_EVENT_PAIRED:           return "PAIRED";
        case PAIR_EVENT_UNPAIRED:         return "UNPAIRED";
        case PAIR_EVENT_RECONNECT_OK:     return "RECONNECT_OK";
        case PAIR_EVENT_TIMEOUT:          return "TIMEOUT";
        case PAIR_EVENT_ERROR:            return "ERROR";
        default:                          return "UNKNOWN";
    }
}

const char *psm_variant_name(psm_variant_t variant)
{
    switch (variant) {
        case PSM_VARIANT_V1_SIMPLE:      return "V1_SIMPLE";
        case PSM_VARIANT_V2_DWELL:       return "V2_DWELL";
        case PSM_VARIANT_V3_DIRECTIONAL: return "V3_DIRECTIONAL";
        default:                          return "UNKNOWN";
    }
}
