/**
 * pairing_state_machine.h — PSM Variants V1, V2, V3 Header
 * Zemi R&D 2026 — Core Activity 1, Experiment 1.2
 */

#pragma once
#include "mlx90393.h"
#include "noise_filter.h"
#include <stdint.h>
#include <math.h>

typedef enum {
    PSM_VARIANT_V1 = 1,  /**< Simple threshold                  */
    PSM_VARIANT_V2,      /**< Threshold + dwell time (150ms)    */
    PSM_VARIANT_V3,      /**< Z-axis dominance lock + dwell     */
} psm_variant_t;

typedef enum {
    PSM_IDLE = 0,
    PSM_SENSING,
    PSM_DWELLING,
    PSM_PAIRED,
    PSM_ERROR,
} pairing_state_t;

typedef enum {
    PSM_EVENT_NONE = 0,
    PSM_EVENT_PAIRED,
    PSM_EVENT_UNPAIRED,
    PSM_EVENT_ERROR,
} psm_event_t;

typedef struct {
    psm_variant_t  variant;
    pairing_state_t state;
    float           threshold_ut;         /**< Pairing field threshold in µT     */
    uint32_t        dwell_ms;             /**< Required field hold time (V2, V3) */
    float           z_dominance_fraction; /**< Z fraction required (V3 only)     */
    int64_t         dwell_start_ms;
    uint32_t        pair_count;
    uint32_t        false_positive_count;
    int64_t         last_pair_time_ms;
} psm_t;

typedef struct {
    uint32_t        pair_count;
    uint32_t        false_positive_count;
    pairing_state_t current_state;
    psm_variant_t   variant;
} psm_stats_t;

void psm_init_v1(psm_t *psm, float threshold_ut);
void psm_init_v2(psm_t *psm, float threshold_ut, uint32_t dwell_ms);
void psm_init_v3(psm_t *psm, float threshold_ut, float z_dominance_fraction);

psm_event_t psm_update_v1(psm_t *psm, mlx90393_data_t *filtered);
psm_event_t psm_update_v2(psm_t *psm, mlx90393_data_t *filtered);
psm_event_t psm_update_v3(psm_t *psm, mlx90393_data_t *filtered);

/** Unified dispatch based on psm->variant */
psm_event_t psm_update(psm_t *psm, mlx90393_data_t *filtered);

/** Populate stats struct for experiment logging */
void psm_get_stats(psm_t *psm, psm_stats_t *stats);
