/**
 * flexible_ferrite.c — Flexible Ferrite Shielding Characterisation
 * Zemi R&D 2026 — Core Activity 2, Experiment 2.2
 *
 * RESEARCH QUESTION:
 *   Does replacing rigid ferrite sheets with flexible ferrite composite material
 *   maintain EMI shielding effectiveness and charging efficiency ≥70% when
 *   the module conforms to a curved wristband form factor?
 *
 * HYPOTHESIS B (CA2):
 *   Flexible ferrite composite material will maintain EMI shielding effectiveness
 *   (field attenuation ≥ rigid baseline) while allowing the charging module to
 *   conform to a curved wristband form factor without efficiency loss below 70%.
 *
 * BACKGROUND (FY25 Gap):
 *   FY25 used a 0.5mm rigid ferrite sheet achieving >75% efficiency at 0mm coil
 *   distance. Introducing flexible PCB coils in FY25 dropped efficiency to >60%
 *   because the rigid ferrite could not curve with the flexible substrate.
 *   The key unknown: can a flexible ferrite composite recover efficiency to ≥70%
 *   while conforming to the wrist curvature, and does its shielding remain
 *   effective against the inductive charging EMI?
 *
 * EXPERIMENT DESIGN — 3 MATERIAL CONFIGURATIONS:
 *   Config M1 — Rigid Ferrite (FY25 baseline): 0.5mm rigid sheet, flat geometry.
 *   Config M2 — Flexible Ferrite Composite:    Flexible ferrite-polymer composite,
 *                                               curved to 40mm radius (Zemi wrist form).
 *   Config M3 — Segmented Rigid Ferrite:       Rigid tiles arranged in curved array
 *                                               with flexible interconnect (intermediate).
 *
 * VARIABLES:
 *   Primary:   Ferrite material configuration (M1/M2/M3)
 *   Secondary: Curvature radius (mm), coil distance (mm), frequency (kHz)
 *   Measured:  Charging efficiency (%), magnetic field attenuation (dB),
 *              EMI leakage (µT at 50mm), peak temperature (°C),
 *              mechanical durability (cycles to >5% efficiency degradation)
 *
 * SUCCESS CRITERION:
 *   Efficiency ≥70% at 5mm coil distance, curved geometry;
 *   EMI attenuation ≥ M1 (rigid baseline); temperature ≤38°C
 *
 * ATO R&D Note: The electromagnetic behaviour of flexible ferrite composites in
 * curved wearable charging geometries is not characterised in existing literature
 * for the specific frequency, coil geometry, and curvature radius of the Zemi device.
 *
 * ATO Substantiation: docs/experiment_log.md — Experiment 2.2
 * GitHub: tkhdwyer82/Zemi-rnd
 * Path:   firmware/core_activity_2_inductive_charging/src/flexible_ferrite.c
 */

#include "flexible_ferrite.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "FLEX_FERRITE";

/* -----------------------------------------------------------------------
 * Characterisation model
 *
 * Real-world efficiency and attenuation data will be measured by a network
 * analyser and power meter during Phase 2 lab testing. This firmware module
 * provides the measurement acquisition, data logging, and comparative
 * analysis framework for that testing.
 *
 * The coupling_model function provides a first-principles estimate of
 * efficiency degradation with curvature based on coil misalignment geometry —
 * used to generate test predictions before hardware testing (hypothesis-phase).
 * ----------------------------------------------------------------------- */

/**
 * @brief Estimate coupling efficiency degradation from curvature misalignment.
 *
 * When a flat coil pair is curved to radius r_mm, the effective axial
 * misalignment between transmitter and receiver coil centres is approximately:
 *   delta = coil_diameter * (1 - cos(arc / r)) / 2
 * where arc is half the coil arc length. Efficiency degrades with misalignment
 * as approximately (1 - (delta / coil_diameter)²).
 *
 * This is a simplified geometric model — actual RF coupling includes higher-
 * order effects that are experimentally characterised in Experiment 2.2.
 */
float ferrite_estimate_coupling_efficiency(float coil_diameter_mm,
                                           float curvature_radius_mm,
                                           float base_efficiency_pct)
{
    if (curvature_radius_mm <= 0.0f) return 0.0f;
    if (curvature_radius_mm > 1000.0f) return base_efficiency_pct; /* Effectively flat */

    float half_arc = coil_diameter_mm * 0.5f;
    float angle_rad = half_arc / curvature_radius_mm;
    float delta_mm = coil_diameter_mm * (1.0f - cosf(angle_rad)) / 2.0f;
    float coupling_factor = 1.0f - (delta_mm / coil_diameter_mm) *
                                    (delta_mm / coil_diameter_mm);
    float estimated_eff = base_efficiency_pct * coupling_factor;

    ESP_LOGD(TAG, "Coupling estimate: r=%.0fmm delta=%.2fmm factor=%.3f eff_est=%.1f%%",
             curvature_radius_mm, delta_mm, coupling_factor, estimated_eff);
    return estimated_eff;
}

/* -----------------------------------------------------------------------
 * Data acquisition and logging framework
 * ----------------------------------------------------------------------- */

void ferrite_config_init(ferrite_config_t *cfg,
                         ferrite_material_t material,
                         float curvature_radius_mm,
                         float coil_distance_mm,
                         float frequency_khz)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(ferrite_config_t));
    cfg->material           = material;
    cfg->curvature_radius_mm = curvature_radius_mm;
    cfg->coil_distance_mm   = coil_distance_mm;
    cfg->frequency_khz      = frequency_khz;
    cfg->sample_count       = 0;

    const char *mat_name = (material == FERRITE_M1_RIGID)    ? "M1-Rigid" :
                           (material == FERRITE_M2_FLEXIBLE) ? "M2-Flexible" :
                                                               "M3-Segmented";
    ESP_LOGI(TAG, "[%s] Init r=%.0fmm d=%.1fmm f=%.0fkHz",
             mat_name, curvature_radius_mm, coil_distance_mm, frequency_khz);
}

void ferrite_record_sample(ferrite_config_t *cfg,
                           float efficiency_pct,
                           float attenuation_db,
                           float emi_leakage_ut,
                           float peak_temp_c)
{
    if (!cfg || cfg->sample_count >= FERRITE_MAX_SAMPLES) {
        ESP_LOGW(TAG, "Sample buffer full or null config");
        return;
    }

    ferrite_sample_t *s = &cfg->samples[cfg->sample_count++];
    s->efficiency_pct    = efficiency_pct;
    s->attenuation_db    = attenuation_db;
    s->emi_leakage_ut    = emi_leakage_ut;
    s->peak_temp_c       = peak_temp_c;

    /* Update running min/max */
    if (cfg->sample_count == 1) {
        cfg->min_efficiency_pct = efficiency_pct;
        cfg->max_temp_c         = peak_temp_c;
    } else {
        if (efficiency_pct < cfg->min_efficiency_pct) cfg->min_efficiency_pct = efficiency_pct;
        if (peak_temp_c    > cfg->max_temp_c)         cfg->max_temp_c = peak_temp_c;
    }

    ESP_LOGD(TAG, "Sample[%u]: eff=%.1f%% att=%.1fdB emi=%.2fµT temp=%.1f°C",
             cfg->sample_count - 1, efficiency_pct, attenuation_db,
             emi_leakage_ut, peak_temp_c);
}

void ferrite_compute_results(ferrite_config_t *cfg, ferrite_result_t *result)
{
    if (!cfg || !result || cfg->sample_count == 0) return;
    memset(result, 0, sizeof(ferrite_result_t));

    result->material   = cfg->material;
    result->n_samples  = cfg->sample_count;

    float sum_eff = 0, sum_att = 0, sum_emi = 0, sum_temp = 0;
    for (uint16_t i = 0; i < cfg->sample_count; i++) {
        sum_eff  += cfg->samples[i].efficiency_pct;
        sum_att  += cfg->samples[i].attenuation_db;
        sum_emi  += cfg->samples[i].emi_leakage_ut;
        sum_temp += cfg->samples[i].peak_temp_c;
    }

    float n = (float)cfg->sample_count;
    result->mean_efficiency_pct  = sum_eff  / n;
    result->mean_attenuation_db  = sum_att  / n;
    result->mean_emi_leakage_ut  = sum_emi  / n;
    result->mean_temp_c          = sum_temp / n;
    result->min_efficiency_pct   = cfg->min_efficiency_pct;
    result->max_temp_c           = cfg->max_temp_c;

    /* Hypothesis B validation check */
    result->hypothesis_b_pass =
        (result->min_efficiency_pct >= 70.0f) &&
        (result->max_temp_c <= 38.0f);

    const char *mat_name = (cfg->material == FERRITE_M1_RIGID)    ? "M1-Rigid" :
                           (cfg->material == FERRITE_M2_FLEXIBLE) ? "M2-Flexible" :
                                                                     "M3-Segmented";
    ESP_LOGI(TAG, "[%s] Results: eff_mean=%.1f%% eff_min=%.1f%% att=%.1fdB temp_max=%.1f°C — %s",
             mat_name,
             result->mean_efficiency_pct, result->min_efficiency_pct,
             result->mean_attenuation_db, result->max_temp_c,
             result->hypothesis_b_pass ? "HYPOTHESIS B: PASS" : "HYPOTHESIS B: FAIL");
}

void ferrite_print_comparison(ferrite_result_t *results, uint8_t n_configs)
{
    printf("\n=== ZEMI R&D 2026 — CA2 Experiment 2.2: Ferrite Material Comparison ===\n");
    printf("%-14s | %-18s | %-18s | %-18s | %-12s | %s\n",
           "Material", "Mean Eff (%)", "Min Eff (%)", "Mean Att (dB)", "Max Temp (°C)", "Hyp-B");
    printf("%-14s-+-%-18s-+-%-18s-+-%-18s-+-%-12s-+-%s\n",
           "--------------", "------------------", "------------------",
           "------------------", "------------", "------");

    for (uint8_t i = 0; i < n_configs; i++) {
        const char *mat = (results[i].material == FERRITE_M1_RIGID)    ? "M1-Rigid" :
                          (results[i].material == FERRITE_M2_FLEXIBLE) ? "M2-Flexible" :
                                                                          "M3-Segmented";
        printf("%-14s | %-18.1f | %-18.1f | %-18.1f | %-12.1f | %s\n",
               mat,
               results[i].mean_efficiency_pct,
               results[i].min_efficiency_pct,
               results[i].mean_attenuation_db,
               results[i].max_temp_c,
               results[i].hypothesis_b_pass ? "PASS" : "FAIL");
    }
    printf("\n=== See docs/experiment_log.md for ATO R&D substantiation ===\n\n");
}
