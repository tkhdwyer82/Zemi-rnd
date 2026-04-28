/**
 * pairing_state_machine.c — Pairing State Machine Variants V1, V2, V3
 * Zemi R&D 2026 — Core Activity 1, Experiment 1.2
 *
 * RESEARCH QUESTION:
 *   Can the MLX90393 maintain ≥95% pairing success at 0–1m in real-world
 *   multi-device environments with an appropriate state machine architecture?
 *
 * HYPOTHESIS A (CA1):
 *   A miniaturised MLX90393 in TPU/silicone housing with EMI shielding and
 *   an optimised state machine will maintain ≥95% pairing success at 0–1m
 *   in real-world multi-device environments.
 *
 * STATE MACHINE VARIANTS:
 *   V1 — Simple Threshold:    Pair when |B| > 5µT. Lowest complexity.
 *                              Expected high false-positive rate in dense env.
 *   V2 — Dwell (Threshold + Hold Time):
 *                              Field must sustain above threshold for ≥150ms
 *                              before pairing is confirmed. Filters transient spikes.
 *   V3 — Directional Z-Lock:  Pairs only when Z-axis contribution > 70% of total
 *                              magnitude, enforcing face-to-face device alignment.
 *                              Most discriminating — lowest false positives expected.
 *
 * VARIABLES:
 *   Primary:   State machine architecture (V1/V2/V3)
 *   Secondary: Pairing threshold (µT), dwell time (ms), Z-axis dominance (%)
 *   Measured:  Pairing success rate (%), false-positive rate (%),
 *              time-to-pair (ms), miss rate at 1m dynamic conditions
 *
 * SUCCESS CRITERION: ≥95% pairing success; <2% false positives; 8hr stability
 *
 * ATO R&D Note: Determining which state machine architecture achieves ≥95%
 * pairing success in a real-world Gen Alpha classroom/playground environment
 * required systematic experimentation — not derivable from published literature
 * or prior adult-oriented wearable research.
 *
 * ATO Substantiation: docs/experiment_log.md — Experiment 1.2
 * GitHub: tkhdwyer82/Zemi-rnd
 * Path:   firmware/core_activity_1_magnetic_sensor/src/pairing_state_machine.c
 */

#include "pairing_state_machine.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "PAIRING_PSM";

/* -----------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------------- */
static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000LL;
}

static const char *state_name(pairing_state_t s)
{
    switch(s) {
        case PSM_IDLE:     return "IDLE";
        case PSM_SENSING:  return "SENSING";
        case PSM_DWELLING: return "DWELLING";
        case PSM_PAIRED:   return "PAIRED";
        case PSM_ERROR:    return "ERROR";
        default:           return "UNKNOWN";
    }
}

/* -----------------------------------------------------------------------
 * VARIANT V1 — Simple Threshold
 * Pair immediately when |B| exceeds PAIR_THRESHOLD_UT.
 * No temporal guard — susceptible to transient EMI events.
 * Benchmark for false-positive measurement vs V2 and V3.
 * ----------------------------------------------------------------------- */
void psm_init_v1(psm_t *psm, float threshold_ut)
{
    memset(psm, 0, sizeof(psm_t));
    psm->variant            = PSM_VARIANT_V1;
    psm->state              = PSM_IDLE;
    psm->threshold_ut       = threshold_ut;
    psm->pair_count         = 0;
    psm->false_positive_count = 0;
    ESP_LOGI(TAG, "[V1-Simple] Init threshold=%.1fµT", threshold_ut);
}

psm_event_t psm_update_v1(psm_t *psm, mlx90393_data_t *filtered)
{
    float mag = filtered->magnitude_ut;

    switch (psm->state) {
        case PSM_IDLE:
            if (mag > psm->threshold_ut) {
                psm->state = PSM_PAIRED;
                psm->pair_count++;
                psm->last_pair_time_ms = now_ms();
                ESP_LOGI(TAG, "[V1] PAIRED |B|=%.1fµT (total=%lu)",
                         mag, (unsigned long)psm->pair_count);
                return PSM_EVENT_PAIRED;
            }
            break;

        case PSM_PAIRED:
            if (mag < (psm->threshold_ut * 0.5f)) {
                /* Hysteresis: unpair at 50% of threshold */
                psm->state = PSM_IDLE;
                ESP_LOGI(TAG, "[V1] UNPAIRED |B|=%.1fµT", mag);
                return PSM_EVENT_UNPAIRED;
            }
            break;

        default:
            psm->state = PSM_IDLE;
            break;
    }
    return PSM_EVENT_NONE;
}

/* -----------------------------------------------------------------------
 * VARIANT V2 — Dwell (Threshold + Hold Time ≥150ms)
 * Field must sustain above threshold for a continuous dwell_ms period
 * before pairing is confirmed. Transient spikes shorter than dwell_ms
 * are rejected without triggering a pairing event.
 * Expected improvement over V1: fewer false positives in dense EMI.
 * ----------------------------------------------------------------------- */
void psm_init_v2(psm_t *psm, float threshold_ut, uint32_t dwell_ms)
{
    memset(psm, 0, sizeof(psm_t));
    psm->variant            = PSM_VARIANT_V2;
    psm->state              = PSM_IDLE;
    psm->threshold_ut       = threshold_ut;
    psm->dwell_ms           = dwell_ms;
    psm->dwell_start_ms     = 0;
    psm->pair_count         = 0;
    psm->false_positive_count = 0;
    ESP_LOGI(TAG, "[V2-Dwell] Init threshold=%.1fµT dwell=%ums", threshold_ut, dwell_ms);
}

psm_event_t psm_update_v2(psm_t *psm, mlx90393_data_t *filtered)
{
    float    mag = filtered->magnitude_ut;
    int64_t  now = now_ms();

    switch (psm->state) {
        case PSM_IDLE:
            if (mag > psm->threshold_ut) {
                psm->state          = PSM_DWELLING;
                psm->dwell_start_ms = now;
                ESP_LOGD(TAG, "[V2] Field detected |B|=%.1fµT — dwelling", mag);
            }
            break;

        case PSM_DWELLING:
            if (mag < psm->threshold_ut) {
                /* Field dropped before dwell elapsed — reject (false positive suppressed) */
                psm->state = PSM_IDLE;
                ESP_LOGD(TAG, "[V2] Field dropped during dwell — rejected (dwell=%lldms)",
                         now - psm->dwell_start_ms);
            } else if ((now - psm->dwell_start_ms) >= (int64_t)psm->dwell_ms) {
                /* Field held for full dwell period — confirm pairing */
                psm->state = PSM_PAIRED;
                psm->pair_count++;
                psm->last_pair_time_ms = now;
                ESP_LOGI(TAG, "[V2] PAIRED after dwell |B|=%.1fµT (total=%lu)",
                         mag, (unsigned long)psm->pair_count);
                return PSM_EVENT_PAIRED;
            }
            break;

        case PSM_PAIRED:
            if (mag < (psm->threshold_ut * 0.5f)) {
                psm->state = PSM_IDLE;
                ESP_LOGI(TAG, "[V2] UNPAIRED |B|=%.1fµT", mag);
                return PSM_EVENT_UNPAIRED;
            }
            break;

        default:
            psm->state = PSM_IDLE;
            break;
    }
    return PSM_EVENT_NONE;
}

/* -----------------------------------------------------------------------
 * VARIANT V3 — Directional Z-Lock (Z-axis dominance ≥70%)
 * Pairs only when the Z-axis magnetic field component accounts for ≥70%
 * of the total field magnitude, enforcing face-to-face device alignment.
 * This discriminates between intentional pairing (aligned devices) and
 * ambient field noise or side-axis interference from adjacent devices.
 *
 * Physics rationale: when two Zemi devices face each other directly, the
 * field from the transmitting device is predominantly along the Z axis
 * (perpendicular to the wrist face). EMI from nearby devices and random
 * environmental fields are typically distributed across X/Y/Z. The Z
 * dominance criterion is a spatial filter with no analogue in software —
 * its effectiveness in a 10-device Gen Alpha environment is the core unknown.
 * ----------------------------------------------------------------------- */
void psm_init_v3(psm_t *psm, float threshold_ut, float z_dominance_fraction)
{
    memset(psm, 0, sizeof(psm_t));
    psm->variant              = PSM_VARIANT_V3;
    psm->state                = PSM_IDLE;
    psm->threshold_ut         = threshold_ut;
    psm->z_dominance_fraction = z_dominance_fraction;   /* e.g. 0.70 for 70% */
    psm->dwell_ms             = 150;  /* V3 also uses a 150ms dwell for robustness */
    psm->dwell_start_ms       = 0;
    psm->pair_count           = 0;
    psm->false_positive_count = 0;
    ESP_LOGI(TAG, "[V3-ZLock] Init threshold=%.1fµT z_dominance=%.0f%% dwell=%ums",
             threshold_ut, z_dominance_fraction * 100.0f, psm->dwell_ms);
}

psm_event_t psm_update_v3(psm_t *psm, mlx90393_data_t *filtered)
{
    float    mag = filtered->magnitude_ut;
    int64_t  now = now_ms();

    /* Compute Z-axis fraction of total magnitude */
    float z_fraction = (mag > 0.1f) ? (fabsf(filtered->z_ut) / mag) : 0.0f;
    bool  z_dominant = (z_fraction >= psm->z_dominance_fraction);

    switch (psm->state) {
        case PSM_IDLE:
            if (mag > psm->threshold_ut && z_dominant) {
                psm->state          = PSM_DWELLING;
                psm->dwell_start_ms = now;
                ESP_LOGD(TAG, "[V3] Z-dominant field |B|=%.1fµT z_frac=%.0f%% — dwelling",
                         mag, z_fraction * 100.0f);
            } else if (mag > psm->threshold_ut && !z_dominant) {
                /* Field strong enough but not Z-dominant — likely ambient/side interference */
                psm->false_positive_count++;
                ESP_LOGD(TAG, "[V3] Non-Z field blocked |B|=%.1fµT z_frac=%.0f%% (fp=%lu)",
                         mag, z_fraction * 100.0f, (unsigned long)psm->false_positive_count);
            }
            break;

        case PSM_DWELLING:
            if (mag < psm->threshold_ut || !z_dominant) {
                psm->state = PSM_IDLE;
                ESP_LOGD(TAG, "[V3] Condition lost during dwell — rejected");
            } else if ((now - psm->dwell_start_ms) >= (int64_t)psm->dwell_ms) {
                psm->state = PSM_PAIRED;
                psm->pair_count++;
                psm->last_pair_time_ms = now;
                ESP_LOGI(TAG, "[V3] PAIRED z_frac=%.0f%% |B|=%.1fµT (total=%lu)",
                         z_fraction * 100.0f, mag, (unsigned long)psm->pair_count);
                return PSM_EVENT_PAIRED;
            }
            break;

        case PSM_PAIRED:
            if (mag < (psm->threshold_ut * 0.5f)) {
                psm->state = PSM_IDLE;
                ESP_LOGI(TAG, "[V3] UNPAIRED |B|=%.1fµT", mag);
                return PSM_EVENT_UNPAIRED;
            }
            break;

        default:
            psm->state = PSM_IDLE;
            break;
    }
    return PSM_EVENT_NONE;
}

/* -----------------------------------------------------------------------
 * Unified dispatch
 * ----------------------------------------------------------------------- */
psm_event_t psm_update(psm_t *psm, mlx90393_data_t *filtered)
{
    switch (psm->variant) {
        case PSM_VARIANT_V1: return psm_update_v1(psm, filtered);
        case PSM_VARIANT_V2: return psm_update_v2(psm, filtered);
        case PSM_VARIANT_V3: return psm_update_v3(psm, filtered);
        default:
            ESP_LOGE(TAG, "Unknown PSM variant — defaulting to V1");
            return psm_update_v1(psm, filtered);
    }
}

void psm_get_stats(psm_t *psm, psm_stats_t *stats)
{
    if (!psm || !stats) return;
    stats->pair_count           = psm->pair_count;
    stats->false_positive_count = psm->false_positive_count;
    stats->current_state        = psm->state;
    stats->variant              = psm->variant;
    ESP_LOGI(TAG, "[%s] Stats: state=%s pairs=%lu fp=%lu",
             psm->variant == PSM_VARIANT_V1 ? "V1" :
             psm->variant == PSM_VARIANT_V2 ? "V2" : "V3",
             state_name(psm->state),
             (unsigned long)stats->pair_count,
             (unsigned long)stats->false_positive_count);
}
