/**
 * ble_coexistence.c — BLE Charging Status + Inductive Coexistence Firmware
 * Zemi R&D 2026 — Core Activity 2, Experiment 2.3
 *
 * RESEARCH QUESTION:
 *   Can a firmware-managed BLE charging status module transmit real-time state
 *   (charging / complete / temperature alert) to a paired smartphone without
 *   creating electromagnetic interference with the magnetic charging coils
 *   operating at 125kHz?
 *
 * HYPOTHESIS C (CA2):
 *   A firmware-managed BLE charging status module operating at 2.4GHz can
 *   coexist with the 125kHz inductive charging field without measurable
 *   degradation in charging efficiency (within ±3% of non-BLE baseline).
 *
 * BACKGROUND (FY25 Gap):
 *   FY25 identified Bluetooth integration as a future enhancement: "we intend
 *   to include wireless communication features in our module, like Bluetooth,
 *   which would enable real-time charging status updates to a smartphone."
 *   The specific unknown is whether simultaneous operation of BLE (2.4GHz) and
 *   inductive charging (125kHz) in the same compact housing creates cross-
 *   interference that degrades charging efficiency or produces EMI artifacts
 *   detectable by the MLX90393 magnetic sensor (CA1 interaction).
 *
 * EXPERIMENT DESIGN — 3 COEXISTENCE MODES:
 *   Mode X1 — Continuous BLE:  BLE advertising at 100ms interval continuously
 *                               during charging. Maximum potential interference.
 *   Mode X2 — Burst BLE:       BLE transmits status burst every 5s, then sleeps.
 *                               Minimises duty cycle. Expected minimal interference.
 *   Mode X3 — Charge-Gated:    BLE transmission is firmware-gated to occur only
 *                               during inductive coil off-time (zero-crossing).
 *                               Most novel — requires precise coil cycle timing.
 *
 * VARIABLES:
 *   Primary:   BLE coexistence mode (X1/X2/X3)
 *   Secondary: BLE advertising interval (ms), TX power (dBm),
 *              coil switching frequency (kHz)
 *   Measured:  Charging efficiency delta (% vs no-BLE baseline),
 *              EMI artefacts in MLX90393 readings (µT RMS increase),
 *              BLE packet delivery ratio (%), status latency (ms)
 *
 * SUCCESS CRITERION:
 *   Charging efficiency delta ≤ ±3% of no-BLE baseline;
 *   BLE packet delivery ≥95%; MLX90393 noise floor increase <5µT RMS
 *
 * ATO R&D Note: The electromagnetic interaction between BLE and 125kHz
 * inductive charging in a compact wrist-worn geometry has no published
 * characterisation for child-safe wearable specifications.
 *
 * ATO Substantiation: docs/experiment_log.md — Experiment 2.3
 * GitHub: tkhdwyer82/Zemi-rnd
 * Path:   firmware/core_activity_2_inductive_charging/src/ble_coexistence.c
 */

#include "ble_coexistence.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "BLE_COEX";

/* BLE GATT characteristic UUIDs for Zemi charging service */
#define ZEMI_CHARGE_SERVICE_UUID     0xFE00
#define ZEMI_CHARGE_STATUS_CHAR_UUID 0xFE01
#define ZEMI_CHARGE_TEMP_CHAR_UUID   0xFE02
#define ZEMI_CHARGE_POWER_CHAR_UUID  0xFE03

static int64_t now_ms(void) { return esp_timer_get_time() / 1000LL; }

/* -----------------------------------------------------------------------
 * Charging status packet construction
 * This is the data payload sent over BLE to the paired smartphone.
 * All three modes send identical packets — only the timing differs.
 * ----------------------------------------------------------------------- */
static void build_status_packet(ble_charge_packet_t *pkt,
                                charging_status_t status,
                                float temp_c,
                                float power_pct,
                                float efficiency_pct)
{
    pkt->status        = status;
    pkt->temp_c_x10    = (int16_t)(temp_c * 10.0f);       /* e.g. 37.5°C → 375 */
    pkt->power_pct_x10 = (uint16_t)(power_pct * 10.0f);
    pkt->efficiency_x10 = (uint16_t)(efficiency_pct * 10.0f);
    pkt->sequence++;
    pkt->timestamp_ms  = (uint32_t)now_ms();

    /* Temperature alert flag */
    if (temp_c > 37.5f) {
        pkt->status = CHARGING_TEMP_ALERT;
    }
}

/* -----------------------------------------------------------------------
 * MODE X1 — Continuous BLE
 * Advertises and notifies at fixed interval regardless of coil state.
 * Acts as worst-case interference baseline.
 * ----------------------------------------------------------------------- */
void ble_coex_init_x1(ble_coex_t *ctx, uint16_t adv_interval_ms)
{
    memset(ctx, 0, sizeof(ble_coex_t));
    ctx->mode              = BLE_MODE_X1_CONTINUOUS;
    ctx->adv_interval_ms   = adv_interval_ms;
    ctx->last_tx_ms        = 0;
    ctx->packets_sent      = 0;
    ctx->packets_delivered = 0;
    ESP_LOGI(TAG, "[X1-Continuous] Init adv_interval=%ums", adv_interval_ms);
}

ble_tx_result_t ble_coex_update_x1(ble_coex_t *ctx,
                                   charging_status_t status,
                                   float temp_c,
                                   float power_pct,
                                   float efficiency_pct)
{
    int64_t now = now_ms();
    if ((now - ctx->last_tx_ms) < (int64_t)ctx->adv_interval_ms) {
        return BLE_TX_SKIPPED;  /* Not yet time to transmit */
    }

    ble_charge_packet_t pkt = {0};
    build_status_packet(&pkt, status, temp_c, power_pct, efficiency_pct);
    ctx->last_tx_ms = now;
    ctx->packets_sent++;

    /*
     * In hardware integration: call esp_ble_gatts_send_indicate() here.
     * In simulation/test: packet is logged for interference measurement.
     */
    ESP_LOGD(TAG, "[X1] TX seq=%u status=%d temp=%.1f°C power=%.0f%% eff=%.1f%%",
             pkt.sequence, pkt.status, temp_c, power_pct, efficiency_pct);

    /* Simulate delivery (hardware measurement will record actual PDR) */
    ctx->packets_delivered++;
    return BLE_TX_SENT;
}

/* -----------------------------------------------------------------------
 * MODE X2 — Burst BLE (5s sleep between bursts)
 * Reduces BLE duty cycle to minimise coil interference.
 * Expected hypothesis: efficiency delta approaches zero.
 * ----------------------------------------------------------------------- */
void ble_coex_init_x2(ble_coex_t *ctx, uint32_t burst_interval_ms)
{
    memset(ctx, 0, sizeof(ble_coex_t));
    ctx->mode              = BLE_MODE_X2_BURST;
    ctx->burst_interval_ms = burst_interval_ms;
    ctx->last_tx_ms        = 0;
    ctx->packets_sent      = 0;
    ctx->packets_delivered = 0;
    ESP_LOGI(TAG, "[X2-Burst] Init burst_interval=%ums", burst_interval_ms);
}

ble_tx_result_t ble_coex_update_x2(ble_coex_t *ctx,
                                   charging_status_t status,
                                   float temp_c,
                                   float power_pct,
                                   float efficiency_pct)
{
    int64_t now = now_ms();
    if ((now - ctx->last_tx_ms) < (int64_t)ctx->burst_interval_ms) {
        return BLE_TX_SKIPPED;
    }

    /* Send a burst of 3 packets in rapid succession then sleep */
    for (int i = 0; i < 3; i++) {
        ble_charge_packet_t pkt = {0};
        build_status_packet(&pkt, status, temp_c, power_pct, efficiency_pct);
        ctx->packets_sent++;
        ctx->packets_delivered++;
        ESP_LOGD(TAG, "[X2] Burst[%d] seq=%u temp=%.1f°C", i, pkt.sequence, temp_c);
        vTaskDelay(pdMS_TO_TICKS(10));  /* 10ms between burst packets */
    }

    ctx->last_tx_ms = now;
    return BLE_TX_SENT;
}

/* -----------------------------------------------------------------------
 * MODE X3 — Charge-Gated BLE (transmit only during coil zero-crossing)
 *
 * Most novel: firmware synchronises BLE transmission to the inductive coil
 * switching cycle, placing BLE TX during the brief window when the coil
 * field is transitioning (zero-crossing) and electromagnetic coupling is
 * at minimum. Requires coil timing signal from the charging controller.
 *
 * This mode requires hardware integration with the BQ500212A transmitter IC
 * to receive a zero-crossing interrupt. In simulation, a synthetic gate
 * signal is used based on the nominal coil switching period.
 * ----------------------------------------------------------------------- */
void ble_coex_init_x3(ble_coex_t *ctx, uint32_t coil_period_us)
{
    memset(ctx, 0, sizeof(ble_coex_t));
    ctx->mode              = BLE_MODE_X3_CHARGE_GATED;
    ctx->coil_period_us    = coil_period_us;  /* 125kHz → 8µs period */
    ctx->last_tx_ms        = 0;
    ctx->packets_sent      = 0;
    ctx->packets_delivered = 0;
    ctx->gate_open         = false;
    ESP_LOGI(TAG, "[X3-Gated] Init coil_period=%uµs (%.1fkHz)",
             coil_period_us, 1000.0f / (coil_period_us / 1000.0f));
}

void ble_coex_signal_zero_crossing(ble_coex_t *ctx)
{
    /* Called from coil zero-crossing ISR or timer callback */
    ctx->gate_open = true;
    ctx->gate_open_time_us = esp_timer_get_time();
}

ble_tx_result_t ble_coex_update_x3(ble_coex_t *ctx,
                                   charging_status_t status,
                                   float temp_c,
                                   float power_pct,
                                   float efficiency_pct)
{
    /* Only transmit if gate is open AND within 3µs of zero-crossing */
    if (!ctx->gate_open) return BLE_TX_GATED;

    int64_t elapsed_us = esp_timer_get_time() - ctx->gate_open_time_us;
    if (elapsed_us > 3) {
        ctx->gate_open = false;
        return BLE_TX_GATED;  /* Gate window expired */
    }

    ble_charge_packet_t pkt = {0};
    build_status_packet(&pkt, status, temp_c, power_pct, efficiency_pct);
    ctx->gate_open = false;
    ctx->packets_sent++;
    ctx->packets_delivered++;
    ctx->last_tx_ms = now_ms();

    ESP_LOGD(TAG, "[X3-Gated] TX at zero-crossing +%lldµs seq=%u status=%d",
             elapsed_us, pkt.sequence, pkt.status);
    return BLE_TX_SENT;
}

/* -----------------------------------------------------------------------
 * Unified dispatch
 * ----------------------------------------------------------------------- */
ble_tx_result_t ble_coex_update(ble_coex_t *ctx,
                                charging_status_t status,
                                float temp_c,
                                float power_pct,
                                float efficiency_pct)
{
    switch (ctx->mode) {
        case BLE_MODE_X1_CONTINUOUS:   return ble_coex_update_x1(ctx, status, temp_c, power_pct, efficiency_pct);
        case BLE_MODE_X2_BURST:        return ble_coex_update_x2(ctx, status, temp_c, power_pct, efficiency_pct);
        case BLE_MODE_X3_CHARGE_GATED: return ble_coex_update_x3(ctx, status, temp_c, power_pct, efficiency_pct);
        default:
            ESP_LOGE(TAG, "Unknown BLE coexistence mode");
            return BLE_TX_ERROR;
    }
}

void ble_coex_get_stats(ble_coex_t *ctx, ble_coex_stats_t *stats)
{
    if (!ctx || !stats) return;
    stats->mode              = ctx->mode;
    stats->packets_sent      = ctx->packets_sent;
    stats->packets_delivered = ctx->packets_delivered;
    stats->delivery_ratio_pct = ctx->packets_sent > 0
        ? (float)ctx->packets_delivered / ctx->packets_sent * 100.0f
        : 0.0f;
    ESP_LOGI(TAG, "BLE Stats mode=%d sent=%lu delivered=%lu PDR=%.1f%%",
             ctx->mode,
             (unsigned long)stats->packets_sent,
             (unsigned long)stats->packets_delivered,
             stats->delivery_ratio_pct);
}
