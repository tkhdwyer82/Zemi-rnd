/**
 * thermal_manager.c — Real-Time Thermal Management Firmware
 * Zemi R&D 2026 — Core Activity 2, Experiment 2.1
 *
 * RESEARCH QUESTION:
 *   Can a compact inductive charging module for the Zemi wearable achieve
 *   validated thermal safety (≤38°C skin contact) through firmware-controlled
 *   power throttling during continuous charging?
 *
 * HYPOTHESIS A (CA2):
 *   Integrating thermal sensors with firmware-controlled power throttling will
 *   maintain device surface temperature ≤38°C during a continuous 2-hour
 *   charging session, without reducing charging efficiency below 70%.
 *
 * BACKGROUND (FY25 Gap):
 *   The FY25 sliding inductive power module experiment achieved ~90% completion.
 *   Real-time thermal management was explicitly identified as unresolved —
 *   the module had no thermal feedback loop, leaving the temperature constraint
 *   (≤38°C for continuous skin contact) untested. This module resolves that gap.
 *
 * EXPERIMENT DESIGN — 3 THROTTLING STRATEGIES:
 *   Strategy 1 — Fixed Ceiling:     Hard-cap transmitter power at 60% when
 *                                    temperature exceeds 35°C. Simple, no overshoot.
 *   Strategy 2 — PID Controller:    Proportional-integral-derivative control loop
 *                                    targeting 36°C setpoint. Smoother, tracks load changes.
 *   Strategy 3 — Predictive Ramp:   Model temperature rise rate; pre-emptively
 *                                    reduce power before threshold is reached.
 *
 * VARIABLES:
 *   Primary:   Throttling strategy (1/2/3)
 *   Secondary: Temperature setpoint (°C), power reduction floor (%),
 *              PID gains (Kp, Ki, Kd), ramp lookahead window (s)
 *   Measured:  Peak skin temperature (°C), charging efficiency (%),
 *              temperature hold stability (°C variance over 2hr),
 *              time above 38°C (seconds)
 *
 * SUCCESS CRITERION:
 *   Peak temperature ≤38°C over 2hr; efficiency ≥70%; time above 38°C = 0s
 *
 * ATO R&D Note: Achieving simultaneous thermal safety and charging efficiency
 * in a child-worn flexible form factor is not resolvable from existing literature —
 * it requires iterative firmware experimentation specific to the Zemi geometry.
 *
 * ATO Substantiation: docs/experiment_log.md — Experiment 2.1
 * GitHub: tkhdwyer82/Zemi-rnd
 * Path:   firmware/core_activity_2_inductive_charging/src/thermal_manager.c
 */

#include "thermal_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>
#include <string.h>

static const char *TAG = "THERMAL_MGR";

/* Safety hard-limit: if any strategy allows temp to exceed this, cut power immediately */
#define THERMAL_EMERGENCY_CUTOFF_C   40.0f
#define THERMAL_RESUME_C             36.0f

/* -----------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------------- */
static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000LL;
}

static float clamp(float val, float lo, float hi)
{
    return val < lo ? lo : (val > hi ? hi : val);
}

/* -----------------------------------------------------------------------
 * STRATEGY 1 — Fixed Ceiling
 *
 * Hard-cap transmitter power duty cycle at THROTTLE_LOW_PCT when the
 * temperature sensor reports above the warning threshold.
 * Simple two-state controller: full power or capped power.
 *
 * Expected behaviour: temperature oscillates around threshold, efficiency
 * drops sharply during throttle events. Sets the baseline against which
 * PID and Predictive strategies are compared.
 * ----------------------------------------------------------------------- */
void thermal_init_s1(thermal_manager_t *tm, float warn_threshold_c, float throttle_low_pct)
{
    memset(tm, 0, sizeof(thermal_manager_t));
    tm->strategy           = THERMAL_STRATEGY_1_FIXED;
    tm->warn_threshold_c   = warn_threshold_c;
    tm->current_power_pct  = 100.0f;
    tm->s1.throttle_low_pct = throttle_low_pct;
    tm->total_energy_mwh   = 0.0f;
    tm->seconds_above_38c  = 0;
    ESP_LOGI(TAG, "[S1-Fixed] Init warn=%.1f°C throttle_low=%.0f%%",
             warn_threshold_c, throttle_low_pct);
}

float thermal_update_s1(thermal_manager_t *tm, float temp_c, float max_power_mw)
{
    /* Emergency cutoff */
    if (temp_c >= THERMAL_EMERGENCY_CUTOFF_C) {
        tm->current_power_pct = 0.0f;
        ESP_LOGE(TAG, "[S1] EMERGENCY CUTOFF temp=%.1f°C", temp_c);
        return 0.0f;
    }

    if (temp_c > tm->warn_threshold_c) {
        tm->current_power_pct = tm->s1.throttle_low_pct;
    } else if (temp_c < (tm->warn_threshold_c - 1.0f)) {
        tm->current_power_pct = 100.0f;  /* Hysteresis: resume at warn-1°C */
    }

    if (temp_c > 38.0f) tm->seconds_above_38c++;

    float output_mw = max_power_mw * (tm->current_power_pct / 100.0f);
    tm->total_energy_mwh += output_mw / 3600.0f;  /* Accumulate energy (assume 1s ticks) */

    ESP_LOGD(TAG, "[S1] temp=%.1f°C power=%.0f%% output=%.1fmW", temp_c, tm->current_power_pct, output_mw);
    return output_mw;
}

/* -----------------------------------------------------------------------
 * STRATEGY 2 — PID Controller
 *
 * Continuous proportional-integral-derivative control targeting a setpoint
 * temperature. The PID adjusts transmitter power smoothly, avoiding the
 * abrupt efficiency drops of the fixed ceiling approach.
 *
 * Gen Alpha wearable tuning:
 *   - Setpoint: 36.0°C (2°C headroom below 38°C limit)
 *   - Kp: 5.0 — proportional response to temperature error
 *   - Ki: 0.1 — integrates over time to correct steady-state offset
 *   - Kd: 2.0 — damps overshoot during rising temperature events
 * These gain values are experimental starting points; Phase 2 lab testing
 * will iterate gains based on measured temperature step responses.
 * ----------------------------------------------------------------------- */
void thermal_init_s2(thermal_manager_t *tm, float setpoint_c, float Kp, float Ki, float Kd)
{
    memset(tm, 0, sizeof(thermal_manager_t));
    tm->strategy           = THERMAL_STRATEGY_2_PID;
    tm->warn_threshold_c   = setpoint_c;
    tm->current_power_pct  = 100.0f;
    tm->s2.setpoint_c      = setpoint_c;
    tm->s2.Kp              = Kp;
    tm->s2.Ki              = Ki;
    tm->s2.Kd              = Kd;
    tm->s2.integral        = 0.0f;
    tm->s2.prev_error      = 0.0f;
    tm->s2.last_update_ms  = now_ms();
    tm->total_energy_mwh   = 0.0f;
    tm->seconds_above_38c  = 0;
    ESP_LOGI(TAG, "[S2-PID] Init setpoint=%.1f°C Kp=%.2f Ki=%.2f Kd=%.2f",
             setpoint_c, Kp, Ki, Kd);
}

float thermal_update_s2(thermal_manager_t *tm, float temp_c, float max_power_mw)
{
    if (temp_c >= THERMAL_EMERGENCY_CUTOFF_C) {
        tm->current_power_pct = 0.0f;
        ESP_LOGE(TAG, "[S2] EMERGENCY CUTOFF temp=%.1f°C", temp_c);
        return 0.0f;
    }

    pid_state_t *p  = &tm->s2;
    int64_t      now = now_ms();
    float        dt  = (float)(now - p->last_update_ms) / 1000.0f;  /* seconds */
    if (dt < 0.001f) dt = 1.0f;  /* Guard against zero-division on first call */
    p->last_update_ms = now;

    /*
     * Error = setpoint - measured temperature.
     * Positive error means temperature is BELOW setpoint — can increase power.
     * Negative error means temperature is ABOVE setpoint — must reduce power.
     */
    float error = p->setpoint_c - temp_c;

    /* Integral with anti-windup clamp ±100 */
    p->integral += error * dt;
    p->integral = clamp(p->integral, -100.0f, 100.0f);

    float derivative = (error - p->prev_error) / dt;
    p->prev_error = error;

    /* PID output: positive = increase power, negative = reduce power */
    float pid_out = p->Kp * error + p->Ki * p->integral + p->Kd * derivative;

    /* Map PID output to power percentage: base 100%, clamp 20–100% */
    tm->current_power_pct = clamp(100.0f + pid_out * 5.0f, 20.0f, 100.0f);

    if (temp_c > 38.0f) tm->seconds_above_38c++;

    float output_mw = max_power_mw * (tm->current_power_pct / 100.0f);
    tm->total_energy_mwh += output_mw / 3600.0f;

    ESP_LOGD(TAG, "[S2-PID] temp=%.1f°C err=%.2f power=%.0f%% output=%.1fmW",
             temp_c, error, tm->current_power_pct, output_mw);
    return output_mw;
}

/* -----------------------------------------------------------------------
 * STRATEGY 3 — Predictive Ramp
 *
 * Measures the rate of temperature rise (dT/dt) over a rolling window
 * and pre-emptively reduces power BEFORE the threshold is reached.
 * The lookahead is: if (current_temp + dT_dt * lookahead_s) > threshold → throttle.
 *
 * This is the most novel strategy — no equivalent has been published for
 * compact wearable inductive charging. The rate-of-rise window (default 30s)
 * and lookahead (default 20s) are experimental parameters.
 * ----------------------------------------------------------------------- */
void thermal_init_s3(thermal_manager_t *tm, float threshold_c,
                     uint8_t window_s, float lookahead_s)
{
    memset(tm, 0, sizeof(thermal_manager_t));
    tm->strategy           = THERMAL_STRATEGY_3_PREDICTIVE;
    tm->warn_threshold_c   = threshold_c;
    tm->current_power_pct  = 100.0f;
    tm->s3.threshold_c     = threshold_c;
    tm->s3.lookahead_s     = lookahead_s;
    tm->s3.window_s        = (window_s > 0 && window_s <= THERMAL_WINDOW_MAX) ? window_s : 30;
    tm->s3.head            = 0;
    tm->s3.count           = 0;
    tm->total_energy_mwh   = 0.0f;
    tm->seconds_above_38c  = 0;
    ESP_LOGI(TAG, "[S3-Predictive] Init threshold=%.1f°C window=%us lookahead=%.0fs",
             threshold_c, tm->s3.window_s, lookahead_s);
}

float thermal_update_s3(thermal_manager_t *tm, float temp_c, float max_power_mw)
{
    if (temp_c >= THERMAL_EMERGENCY_CUTOFF_C) {
        tm->current_power_pct = 0.0f;
        ESP_LOGE(TAG, "[S3] EMERGENCY CUTOFF temp=%.1f°C", temp_c);
        return 0.0f;
    }

    predictive_state_t *s = &tm->s3;

    /* Store temperature in rolling window */
    s->temp_history[s->head] = temp_c;
    s->head = (s->head + 1) % s->window_s;
    if (s->count < s->window_s) s->count++;

    /* Estimate dT/dt: linear regression over filled window vs constant-1°C baseline */
    float dT_dt = 0.0f;
    if (s->count >= 2) {
        /* Simple: use oldest vs newest sample in window */
        uint8_t oldest_idx = (s->head) % s->count;
        float oldest_temp  = s->temp_history[oldest_idx];
        float window_time  = (float)(s->count - 1);
        dT_dt = (temp_c - oldest_temp) / window_time;  /* °C/second */
    }

    /* Predict temperature at lookahead_s from now */
    float predicted_temp = temp_c + dT_dt * s->lookahead_s;

    if (predicted_temp > s->threshold_c) {
        /* Pre-emptive throttle: scale down power proportionally to overshoot */
        float overshoot = predicted_temp - s->threshold_c;
        float throttle  = clamp(1.0f - overshoot * 0.1f, 0.20f, 0.90f);
        tm->current_power_pct = throttle * 100.0f;
        ESP_LOGD(TAG, "[S3] Predictive throttle: predicted=%.1f°C dT/dt=%.3f°C/s power=%.0f%%",
                 predicted_temp, dT_dt, tm->current_power_pct);
    } else {
        tm->current_power_pct = 100.0f;
    }

    if (temp_c > 38.0f) tm->seconds_above_38c++;

    float output_mw = max_power_mw * (tm->current_power_pct / 100.0f);
    tm->total_energy_mwh += output_mw / 3600.0f;
    return output_mw;
}

/* -----------------------------------------------------------------------
 * Unified dispatch
 * ----------------------------------------------------------------------- */
float thermal_update(thermal_manager_t *tm, float temp_c, float max_power_mw)
{
    switch (tm->strategy) {
        case THERMAL_STRATEGY_1_FIXED:      return thermal_update_s1(tm, temp_c, max_power_mw);
        case THERMAL_STRATEGY_2_PID:        return thermal_update_s2(tm, temp_c, max_power_mw);
        case THERMAL_STRATEGY_3_PREDICTIVE: return thermal_update_s3(tm, temp_c, max_power_mw);
        default:
            ESP_LOGE(TAG, "Unknown thermal strategy");
            return 0.0f;
    }
}

void thermal_get_stats(thermal_manager_t *tm, thermal_stats_t *stats)
{
    if (!tm || !stats) return;
    stats->strategy              = tm->strategy;
    stats->current_power_pct     = tm->current_power_pct;
    stats->total_energy_mwh      = tm->total_energy_mwh;
    stats->seconds_above_38c     = tm->seconds_above_38c;
    ESP_LOGI(TAG, "Stats: strategy=%d power=%.0f%% energy=%.2fmWh t>38°C=%us",
             tm->strategy, tm->current_power_pct,
             tm->total_energy_mwh, tm->seconds_above_38c);
}
