/**
 * thermal_manager.h — Real-Time Thermal Management Header
 * Zemi R&D 2026 — Core Activity 2, Experiment 2.1
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

#define THERMAL_WINDOW_MAX  60  /**< Max rolling window in seconds */

typedef enum {
    THERMAL_STRATEGY_1_FIXED = 1,   /**< Hard power ceiling at threshold    */
    THERMAL_STRATEGY_2_PID,         /**< PID controller targeting setpoint  */
    THERMAL_STRATEGY_3_PREDICTIVE,  /**< Rate-of-rise predictive throttling */
} thermal_strategy_t;

typedef struct {
    float throttle_low_pct;  /**< Power % to apply when threshold exceeded */
} fixed_state_t;

typedef struct {
    float   setpoint_c;
    float   Kp, Ki, Kd;
    float   integral;
    float   prev_error;
    int64_t last_update_ms;
} pid_state_t;

typedef struct {
    float   threshold_c;
    float   lookahead_s;
    uint8_t window_s;
    float   temp_history[THERMAL_WINDOW_MAX];
    uint8_t head;
    uint8_t count;
} predictive_state_t;

typedef struct {
    thermal_strategy_t strategy;
    float              warn_threshold_c;
    float              current_power_pct;
    float              total_energy_mwh;
    uint32_t           seconds_above_38c;
    union {
        fixed_state_t       s1;
        pid_state_t         s2;
        predictive_state_t  s3;
    };
} thermal_manager_t;

typedef struct {
    thermal_strategy_t strategy;
    float              current_power_pct;
    float              total_energy_mwh;
    uint32_t           seconds_above_38c;
} thermal_stats_t;

void  thermal_init_s1(thermal_manager_t *tm, float warn_threshold_c, float throttle_low_pct);
void  thermal_init_s2(thermal_manager_t *tm, float setpoint_c, float Kp, float Ki, float Kd);
void  thermal_init_s3(thermal_manager_t *tm, float threshold_c, uint8_t window_s, float lookahead_s);

/** Compute and apply thermal management; returns actual output power (mW) */
float thermal_update(thermal_manager_t *tm, float temp_c, float max_power_mw);

/** Populate stats for experiment log */
void  thermal_get_stats(thermal_manager_t *tm, thermal_stats_t *stats);
