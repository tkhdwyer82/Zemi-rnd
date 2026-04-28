/**
 * ble_coexistence.h — BLE Charging Status Coexistence Header
 * Zemi R&D 2026 — Core Activity 2, Experiment 2.3
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CHARGING_IDLE        = 0,
    CHARGING_ACTIVE,
    CHARGING_COMPLETE,
    CHARGING_TEMP_ALERT,
    CHARGING_ERROR,
} charging_status_t;

typedef enum {
    BLE_MODE_X1_CONTINUOUS   = 1,  /**< Continuous BLE, worst-case interference  */
    BLE_MODE_X2_BURST,             /**< Burst every 5s, low duty cycle            */
    BLE_MODE_X3_CHARGE_GATED,      /**< TX gated to coil zero-crossing only       */
} ble_mode_t;

typedef enum {
    BLE_TX_SENT    = 0,
    BLE_TX_SKIPPED,
    BLE_TX_GATED,
    BLE_TX_ERROR,
} ble_tx_result_t;

typedef struct {
    charging_status_t status;
    int16_t           temp_c_x10;        /**< Temperature × 10 (fixed point)   */
    uint16_t          power_pct_x10;     /**< Power % × 10                     */
    uint16_t          efficiency_x10;    /**< Efficiency % × 10                */
    uint32_t          sequence;
    uint32_t          timestamp_ms;
} ble_charge_packet_t;

typedef struct {
    ble_mode_t  mode;
    uint16_t    adv_interval_ms;
    uint32_t    burst_interval_ms;
    uint32_t    coil_period_us;
    int64_t     last_tx_ms;
    int64_t     gate_open_time_us;
    bool        gate_open;
    uint32_t    packets_sent;
    uint32_t    packets_delivered;
} ble_coex_t;

typedef struct {
    ble_mode_t mode;
    uint32_t   packets_sent;
    uint32_t   packets_delivered;
    float      delivery_ratio_pct;
} ble_coex_stats_t;

void           ble_coex_init_x1(ble_coex_t *ctx, uint16_t adv_interval_ms);
void           ble_coex_init_x2(ble_coex_t *ctx, uint32_t burst_interval_ms);
void           ble_coex_init_x3(ble_coex_t *ctx, uint32_t coil_period_us);

/** Call from coil zero-crossing ISR — signals gate open for X3 mode */
void           ble_coex_signal_zero_crossing(ble_coex_t *ctx);

ble_tx_result_t ble_coex_update(ble_coex_t *ctx, charging_status_t status,
                                float temp_c, float power_pct, float efficiency_pct);

void           ble_coex_get_stats(ble_coex_t *ctx, ble_coex_stats_t *stats);
