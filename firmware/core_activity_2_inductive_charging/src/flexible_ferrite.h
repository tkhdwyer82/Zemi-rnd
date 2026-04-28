/**
 * flexible_ferrite.h — Flexible Ferrite Shielding Characterisation Header
 * Zemi R&D 2026 — Core Activity 2, Experiment 2.2
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

#define FERRITE_MAX_SAMPLES  200

typedef enum {
    FERRITE_M1_RIGID    = 1,  /**< FY25 rigid 0.5mm ferrite sheet baseline */
    FERRITE_M2_FLEXIBLE,      /**< Flexible ferrite-polymer composite       */
    FERRITE_M3_SEGMENTED,     /**< Segmented rigid tiles + flexible joins   */
} ferrite_material_t;

typedef struct {
    float efficiency_pct;   /**< Measured charging efficiency (%)       */
    float attenuation_db;   /**< Magnetic field attenuation (dB)        */
    float emi_leakage_ut;   /**< EMI leakage at 50mm distance (µT)      */
    float peak_temp_c;      /**< Peak surface temperature (°C)          */
} ferrite_sample_t;

typedef struct {
    ferrite_material_t material;
    float              curvature_radius_mm;
    float              coil_distance_mm;
    float              frequency_khz;
    ferrite_sample_t   samples[FERRITE_MAX_SAMPLES];
    uint16_t           sample_count;
    float              min_efficiency_pct;
    float              max_temp_c;
} ferrite_config_t;

typedef struct {
    ferrite_material_t material;
    uint16_t           n_samples;
    float              mean_efficiency_pct;
    float              min_efficiency_pct;
    float              mean_attenuation_db;
    float              mean_emi_leakage_ut;
    float              mean_temp_c;
    float              max_temp_c;
    bool               hypothesis_b_pass;  /**< true if eff≥70% AND temp≤38°C */
} ferrite_result_t;

float ferrite_estimate_coupling_efficiency(float coil_diameter_mm,
                                           float curvature_radius_mm,
                                           float base_efficiency_pct);

void ferrite_config_init(ferrite_config_t *cfg, ferrite_material_t material,
                         float curvature_radius_mm, float coil_distance_mm,
                         float frequency_khz);

void ferrite_record_sample(ferrite_config_t *cfg, float efficiency_pct,
                           float attenuation_db, float emi_leakage_ut, float peak_temp_c);

void ferrite_compute_results(ferrite_config_t *cfg, ferrite_result_t *result);

void ferrite_print_comparison(ferrite_result_t *results, uint8_t n_configs);
