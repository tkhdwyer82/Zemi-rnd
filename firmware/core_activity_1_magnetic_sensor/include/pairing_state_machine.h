/**
 * @file pairing_state_machine.h
 * @brief Magnetic Pairing State Machine — Zemi Wearable
 *
 * Zemi R&D 2026 — Core Activity 1, Experiment 1.2
 *
 * Research question:
 *   What is the optimal firmware state machine architecture for reliable,
 *   low-latency device pairing using the 3D magnetic sensor?
 *
 * Three state machine variants are implemented for comparative testing:
 *   V1 — Simple threshold: pair if magnitude > PAIR_THRESHOLD_uT
 *   V2 — Dwell + confirm: magnitude must hold above threshold for N ms
 *   V3 — Directional lock: pairing only when Z-axis dominant (close approach)
 *
 * All variants share the same state enum and callback interface so the
 * test harness can swap between them at runtime.
 *
 * Contractor: Timothy Dwyer (TKD Research and Consulting)
 * Company:    Zemi Pty Ltd
 */

#ifndef PAIRING_STATE_MACHINE_H
#define PAIRING_STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>
#include "mlx90393.h"
#include "noise_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Pairing Thresholds ───────────────────────────────────────────────
// Derived from FY25 range experiments. Field strength at 1m ≈ 0.1–2uT
// ambient; a Zemi device at <1m produces >> 5uT on the 3D axis.
#define PAIR_THRESHOLD_uT           5.0f    /**< Min magnitude to consider pairing */
#define PAIR_DWELL_MS               150     /**< V2: Hold time for dwell confirmation */
#define PAIR_Z_DOMINANCE_RATIO      0.7f    /**< V3: Z must be > 70% of magnitude */
#define PAIR_TIMEOUT_MS             5000    /**< Max time in DETECTING before timeout */
#define PAIR_RECONNECT_TIMEOUT_MS   2000    /**< Reconnect window after brief loss */
#define PAIR_MAX_DISTANCE_uT        80.0f   /**< Approx upper bound for valid pair field */

// ── State Machine Variant ────────────────────────────────────────────
typedef enum {
    PSM_VARIANT_V1_SIMPLE       = 0,    /**< Iteration 1: Simple threshold */
    PSM_VARIANT_V2_DWELL        = 1,    /**< Iteration 2: Dwell + confirm */
    PSM_VARIANT_V3_DIRECTIONAL  = 2,    /**< Iteration 3: Z-axis directional */
} psm_variant_t;

// ── Pairing States ───────────────────────────────────────────────────
typedef enum {
    PAIR_STATE_IDLE         = 0,    /**< Not scanning */
    PAIR_STATE_SCANNING     = 1,    /**< Scanning for a pairable device */
    PAIR_STATE_DETECTING    = 2,    /**< Field detected, confirming */
    PAIR_STATE_PAIRED       = 3,    /**< Paired and connected */
    PAIR_STATE_RECONNECTING = 4,    /**< Brief field loss, attempting reconnect */
    PAIR_STATE_ERROR        = 5,    /**< Error / timeout */
} pair_state_t;

// ── Pairing Event (returned to application layer) ───────────────────
typedef enum {
    PAIR_EVENT_NONE             = 0,
    PAIR_EVENT_SCANNING_STARTED = 1,
    PAIR_EVENT_FIELD_DETECTED   = 2,
    PAIR_EVENT_PAIRED           = 3,
    PAIR_EVENT_UNPAIRED         = 4,
    PAIR_EVENT_RECONNECT_OK     = 5,
    PAIR_EVENT_TIMEOUT          = 6,
    PAIR_EVENT_ERROR            = 7,
} pair_event_t;

// ── R&D Telemetry (logged per session for ATO substantiation) ───────
typedef struct {
    uint32_t    total_pair_attempts;
    uint32_t    successful_pairs;
    uint32_t    failed_pairs;
    uint32_t    timeouts;
    uint32_t    false_positives;    /**< Paired with no device in range */
    uint32_t    reconnect_successes;
    float       avg_pair_latency_ms;
    float       min_pair_latency_ms;
    float       max_pair_latency_ms;
} psm_telemetry_t;

// ── State Machine Handle ─────────────────────────────────────────────
typedef struct {
    psm_variant_t       variant;
    pair_state_t        state;
    zemi_filter_state_t filter;

    // Internal timing
    int64_t     state_entry_time_us;
    int64_t     dwell_start_us;     /**< V2: when dwell began */
    float       dwell_peak_uT;      /**< V2: peak magnitude during dwell */

    // Telemetry
    psm_telemetry_t telemetry;

    // Last processed measurement
    mlx90393_measurement_t last_measurement;
    float                  last_magnitude_uT;
} psm_handle_t;

// ── Callback type ────────────────────────────────────────────────────
typedef void (*psm_event_cb_t)(pair_event_t event,
                                const psm_handle_t *handle,
                                void *user_data);

// ── Public API ───────────────────────────────────────────────────────

/**
 * @brief Initialise the pairing state machine.
 * @param handle        PSM handle to populate.
 * @param variant       Which iteration to use (V1/V2/V3).
 * @param filter_type   Which noise filter to pair with.
 * @param cb            Event callback (nullable).
 * @param user_data     Passed through to callback.
 */
void psm_init(psm_handle_t *handle,
              psm_variant_t variant,
              zemi_filter_type_t filter_type,
              psm_event_cb_t cb,
              void *user_data);

/**
 * @brief Start scanning for a pairable device.
 */
void psm_start_scanning(psm_handle_t *handle);

/**
 * @brief Feed a new raw sensor measurement into the state machine.
 *        Must be called at the sensor sample rate (e.g. 50Hz in burst mode).
 * @return The event generated by this update (PAIR_EVENT_NONE if no change).
 */
pair_event_t psm_update(psm_handle_t *handle,
                         const mlx90393_measurement_t *raw,
                         psm_event_cb_t cb, void *user_data);

/**
 * @brief Manually unpair and return to IDLE.
 */
void psm_unpair(psm_handle_t *handle);

/**
 * @brief Get current state.
 */
pair_state_t psm_get_state(const psm_handle_t *handle);

/**
 * @brief Get a copy of the current telemetry snapshot.
 */
psm_telemetry_t psm_get_telemetry(const psm_handle_t *handle);

/**
 * @brief Reset telemetry counters between test runs.
 */
void psm_reset_telemetry(psm_handle_t *handle);

/**
 * @brief State name string for logging.
 */
const char *psm_state_name(pair_state_t state);

/**
 * @brief Event name string for logging.
 */
const char *psm_event_name(pair_event_t event);

/**
 * @brief Variant name string for logging.
 */
const char *psm_variant_name(psm_variant_t variant);

#ifdef __cplusplus
}
#endif

#endif /* PAIRING_STATE_MACHINE_H */
