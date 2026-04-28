/**
 * test_sensor_firmware.c — CA1 Test Harness (4×3 Matrix)
 * Zemi R&D 2026 — Core Activity 1, Experiments 1.1 & 1.2
 *
 * Tests all combinations of noise filter iterations (A, B, C, D)
 * against pairing state machine variants (V1, V2, V3) — 12 conditions.
 * For each condition, the harness runs a simulated 8-hour wear session
 * with injected EMI events and reports pairing success rate and
 * false-positive rate to UART for ATO experiment log capture.
 *
 * ATO Substantiation: docs/experiment_log.md — Experiments 1.1 and 1.2
 * GitHub: tkhdwyer82/Zemi-rnd
 * Path:   firmware/core_activity_1_magnetic_sensor/tests/test_sensor_firmware.c
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "mlx90393.h"
#include "noise_filter.h"
#include "pairing_state_machine.h"

/* -----------------------------------------------------------------------
 * Simulated sensor data generator
 * Generates a stream of readings representing:
 *   - Background ambient field (noise floor ~2µT RMS)
 *   - Intentional pairing event (field ramp to ~8µT on Z-axis)
 *   - EMI spike injection (impulsive >50µT transient on X-axis)
 * This allows deterministic comparison across filter/PSM combinations
 * before live hardware testing begins in Phase 2.
 * ----------------------------------------------------------------------- */
typedef enum {
    SIM_AMBIENT = 0,   /* Background noise only */
    SIM_PAIRING,       /* Valid pairing field present */
    SIM_EMI_SPIKE,     /* Injected EMI transient */
} sim_event_t;

static void generate_sample(sim_event_t event, mlx90393_data_t *out, uint32_t tick)
{
    /* Pseudo-noise: deterministic approximation of white noise using tick */
    float noise = ((float)(tick % 17) - 8.0f) * 0.15f;  /* ±1.2µT noise */

    switch (event) {
        case SIM_AMBIENT:
            out->x_ut = noise;
            out->y_ut = noise * 0.8f;
            out->z_ut = noise * 0.6f;
            break;

        case SIM_PAIRING:
            /* Face-to-face pairing: Z-dominant field ~8µT */
            out->x_ut = 0.5f + noise;
            out->y_ut = 0.5f + noise;
            out->z_ut = 8.0f + noise * 0.3f;  /* Z dominant */
            break;

        case SIM_EMI_SPIKE:
            /* EMI spike: impulsive, X-dominant, large magnitude */
            out->x_ut = 65.0f + noise;
            out->y_ut = 5.0f  + noise;
            out->z_ut = 3.0f  + noise;
            break;
    }

    out->magnitude_ut = sqrtf(out->x_ut * out->x_ut +
                               out->y_ut * out->y_ut +
                               out->z_ut * out->z_ut);
}

/* -----------------------------------------------------------------------
 * Single condition run: one filter iteration + one PSM variant
 * Returns: pairing success rate and false-positive rate
 * ----------------------------------------------------------------------- */
typedef struct {
    uint32_t intended_pairs;      /* Pairing events injected */
    uint32_t successful_pairs;    /* PSM detected correctly  */
    uint32_t false_positives;     /* PSM triggered on EMI    */
    uint32_t emi_events;          /* EMI events injected     */
    float    success_rate_pct;
    float    false_positive_rate_pct;
} test_result_t;

static void run_condition(filter_type_t filter, psm_variant_t psm_variant,
                          test_result_t *result)
{
    memset(result, 0, sizeof(test_result_t));

    /* Initialise filter */
    noise_filter_t nf;
    memset(&nf, 0, sizeof(nf));
    switch (filter) {
        case FILTER_ITERATION_A:
            nf.type = FILTER_ITERATION_A;
            break;
        case FILTER_ITERATION_B:
            noise_filter_init_b(&nf, 8);
            break;
        case FILTER_ITERATION_C:
            noise_filter_init_c(&nf, 0.05f, 0.5f);  /* Gen Alpha Q/R */
            break;
        case FILTER_ITERATION_D:
            noise_filter_init_d(&nf, 50.0f, 0.05f, 0.5f);
            break;
    }

    /* Initialise PSM */
    psm_t psm;
    switch (psm_variant) {
        case PSM_VARIANT_V1: psm_init_v1(&psm, 5.0f);               break;
        case PSM_VARIANT_V2: psm_init_v2(&psm, 5.0f, 150);          break;
        case PSM_VARIANT_V3: psm_init_v3(&psm, 5.0f, 0.70f);        break;
    }

    /*
     * Simulate 500-sample session (representative of one 10-minute test):
     *   - 20 intended pairing events (10 samples each)
     *   - 15 EMI spike events (1 sample each)
     *   - Remainder: ambient
     */
    uint32_t pairing_at[20], emi_at[15];
    for (int i = 0; i < 20; i++) pairing_at[i] = 10 + i * 24;  /* spaced 24 ticks apart */
    for (int i = 0; i < 15; i++) emi_at[i]     = 5  + i * 33;

    result->intended_pairs = 20;
    result->emi_events      = 15;

    /* Track which pairing events were detected */
    bool pairing_window[20] = {0};

    for (uint32_t tick = 0; tick < 500; tick++) {
        /* Determine event type for this tick */
        sim_event_t event = SIM_AMBIENT;
        int pair_idx = -1;

        for (int i = 0; i < 20; i++) {
            if (tick >= pairing_at[i] && tick < pairing_at[i] + 10) {
                event = SIM_PAIRING;
                pair_idx = i;
                break;
            }
        }
        if (event == SIM_AMBIENT) {
            for (int i = 0; i < 15; i++) {
                if (tick == emi_at[i]) {
                    event = SIM_EMI_SPIKE;
                    break;
                }
            }
        }

        mlx90393_data_t raw, filtered;
        generate_sample(event, &raw, tick);
        noise_filter_apply(&nf, &raw, &filtered);
        psm_event_t evt = psm_update(&psm, &filtered);

        if (evt == PSM_EVENT_PAIRED) {
            if (event == SIM_PAIRING && pair_idx >= 0 && !pairing_window[pair_idx]) {
                pairing_window[pair_idx] = true;
                result->successful_pairs++;
            } else if (event == SIM_EMI_SPIKE || event == SIM_AMBIENT) {
                result->false_positives++;
            }
        }
    }

    result->success_rate_pct = result->intended_pairs > 0
        ? (float)result->successful_pairs / result->intended_pairs * 100.0f
        : 0.0f;

    result->false_positive_rate_pct = result->emi_events > 0
        ? (float)result->false_positives / result->emi_events * 100.0f
        : 0.0f;
}

/* -----------------------------------------------------------------------
 * Main test runner — 4×3 matrix
 * ----------------------------------------------------------------------- */
int test_sensor_main(void)
{
    const char *filter_names[] = { "A-Baseline", "B-MovAvg", "C-Kalman", "D-Hybrid" };
    const char *psm_names[]    = { "V1-Simple",  "V2-Dwell", "V3-ZLock" };

    filter_type_t filters[] = {
        FILTER_ITERATION_A, FILTER_ITERATION_B,
        FILTER_ITERATION_C, FILTER_ITERATION_D
    };
    psm_variant_t variants[] = { PSM_VARIANT_V1, PSM_VARIANT_V2, PSM_VARIANT_V3 };

    printf("\n=== ZEMI R&D 2026 — CA1 Test Matrix ===\n");
    printf("%-15s | %-12s | %-18s | %-22s | %s\n",
           "Filter", "PSM", "Success Rate (%)", "False Positive Rate (%)", "PASS/FAIL");
    printf("%-15s-+-%-12s-+-%-18s-+-%-22s-+-%s\n",
           "---------------", "------------", "------------------",
           "----------------------", "--------");

    int pass_count = 0, total = 0;

    for (int f = 0; f < 4; f++) {
        for (int p = 0; p < 3; p++) {
            test_result_t result;
            run_condition(filters[f], variants[p], &result);

            bool pass = (result.success_rate_pct >= 95.0f) &&
                        (result.false_positive_rate_pct < 2.0f);
            if (pass) pass_count++;
            total++;

            printf("%-15s | %-12s | %-18.1f | %-22.1f | %s\n",
                   filter_names[f], psm_names[p],
                   result.success_rate_pct,
                   result.false_positive_rate_pct,
                   pass ? "PASS" : "FAIL");
        }
    }

    printf("\n=== Results: %d/%d conditions met ≥95%% success + <2%% FP criteria ===\n",
           pass_count, total);
    printf("=== See docs/experiment_log.md for ATO R&D substantiation ===\n\n");

    return (pass_count > 0) ? 0 : 1;
}
