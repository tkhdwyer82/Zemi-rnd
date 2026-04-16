/**
 * @file test_sensor_firmware.c
 * @brief R&D Test Harness — Core Activity 1, Experiments 1.1 & 1.2
 *
 * Zemi R&D 2026
 * Contractor: Timothy Dwyer (TKD Research and Consulting)
 * Company:    Zemi Pty Ltd
 *
 * This test harness runs all filter iterations (A–D) and all PSM variants
 * (V1–V3) in a structured matrix. Results are logged to serial in CSV
 * format for analysis in the R&D experiment log (ATO substantiation).
 *
 * Usage: Flash to Zemi prototype, open serial monitor at 115200 baud.
 * Output format: CSV rows tagged with filter, variant, run_id, event,
 * magnitude, latency_ms, timestamp.
 *
 * Build: ESP-IDF v5.x (idf.py build flash monitor)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c.h"

#include "mlx90393.h"
#include "noise_filter.h"
#include "pairing_state_machine.h"

static const char *TAG = "ZEMI_TEST";

// ── Hardware config (Zemi prototype PCB) ────────────────────────────
#define I2C_MASTER_SCL_IO       22
#define I2C_MASTER_SDA_IO       21
#define I2C_MASTER_FREQ_HZ      400000
#define I2C_PORT                I2C_NUM_0

// ── Test parameters ──────────────────────────────────────────────────
#define TEST_SAMPLE_RATE_HZ     50          /**< 50 Hz = 20ms per sample */
#define TEST_DURATION_S         30          /**< 30s per run */
#define TEST_SAMPLES_PER_RUN    (TEST_SAMPLE_RATE_HZ * TEST_DURATION_S)

// ── R&D run tracking ─────────────────────────────────────────────────
static uint32_t g_run_id = 0;

// ── PSM event callback ───────────────────────────────────────────────
static void _on_pair_event(pair_event_t event,
                            const psm_handle_t *handle,
                            void *user_data)
{
    (void)user_data;
    // CSV log format:
    // RUN_ID, FILTER, PSM_VARIANT, EVENT, STATE, MAG_uT, TIMESTAMP_MS
    printf("CSV,%lu,%s,%s,%s,%s,%.2f,%lld\n",
           (unsigned long)g_run_id,
           zemi_filter_name(handle->filter.type),
           psm_variant_name(handle->variant),
           psm_event_name(event),
           psm_state_name(psm_get_state(handle)),
           handle->last_magnitude_uT,
           esp_timer_get_time() / 1000LL);
}

// ── I2C initialisation ───────────────────────────────────────────────
static esp_err_t _i2c_init(void)
{
    i2c_config_t conf = {
        .mode               = I2C_MODE_MASTER,
        .sda_io_num         = I2C_MASTER_SDA_IO,
        .scl_io_num         = I2C_MASTER_SCL_IO,
        .sda_pullup_en      = GPIO_PULLUP_ENABLE,
        .scl_pullup_en      = GPIO_PULLUP_ENABLE,
        .master.clk_speed   = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    return i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
}

// ── Single experiment run ─────────────────────────────────────────────
static void _run_experiment(mlx90393_dev_t *sensor,
                             zemi_filter_type_t filter,
                             psm_variant_t variant)
{
    g_run_id++;
    ESP_LOGI(TAG, "=== RUN %lu: filter=%s  psm=%s ===",
             (unsigned long)g_run_id,
             zemi_filter_name(filter),
             psm_variant_name(variant));

    psm_handle_t psm;
    psm_init(&psm, variant, filter, _on_pair_event, NULL);
    psm_start_scanning(&psm);

    // Start sensor burst mode
    ESP_ERROR_CHECK(mlx90393_start_burst(sensor, MLX90393_AXIS_XYZT));
    vTaskDelay(pdMS_TO_TICKS(20)); // first burst settle

    int64_t run_start = esp_timer_get_time();
    uint32_t sample_count = 0;

    while (sample_count < TEST_SAMPLES_PER_RUN) {
        mlx90393_measurement_t raw;
        esp_err_t ret = mlx90393_read_burst(sensor, &raw);

        if (ret == ESP_OK && raw.valid) {
            psm_update(&psm, &raw, _on_pair_event, NULL);
            sample_count++;
        } else {
            ESP_LOGW(TAG, "Read error or invalid sample (sample %lu)", (unsigned long)sample_count);
        }

        vTaskDelay(pdMS_TO_TICKS(1000 / TEST_SAMPLE_RATE_HZ));
    }

    mlx90393_exit(sensor);

    // ── Print telemetry summary (CSV + human-readable) ───────────────
    psm_telemetry_t t = psm_get_telemetry(&psm);
    int64_t elapsed_ms = (esp_timer_get_time() - run_start) / 1000LL;

    float success_rate = 0.0f;
    if (t.total_pair_attempts > 0) {
        success_rate = (float)t.successful_pairs /
                       (float)t.total_pair_attempts * 100.0f;
    }
    float fp_rate = 0.0f;
    if ((t.successful_pairs + t.false_positives) > 0) {
        fp_rate = (float)t.false_positives /
                  (float)(t.successful_pairs + t.false_positives) * 100.0f;
    }

    printf("\nSUMMARY,run=%lu,filter=%s,variant=%s,"
           "samples=%lu,duration_ms=%lld,"
           "pairs=%lu,success_rate=%.1f%%,"
           "false_pos=%lu,fp_rate=%.1f%%,"
           "timeouts=%lu,reconnects=%lu,"
           "latency_avg=%.1fms,latency_min=%.1fms,latency_max=%.1fms,"
           "filter_rejected=%lu\n",
           (unsigned long)g_run_id,
           zemi_filter_name(filter),
           psm_variant_name(variant),
           (unsigned long)sample_count,
           elapsed_ms,
           (unsigned long)t.successful_pairs,
           success_rate,
           (unsigned long)t.false_positives,
           fp_rate,
           (unsigned long)t.timeouts,
           (unsigned long)t.reconnect_successes,
           t.avg_pair_latency_ms,
           t.min_pair_latency_ms,
           t.max_pair_latency_ms,
           (unsigned long)psm.filter.samples_rejected);

    ESP_LOGI(TAG, "Run complete. success=%.1f%% fp=%.1f%% avg_latency=%.1fms",
             success_rate, fp_rate, t.avg_pair_latency_ms);

    vTaskDelay(pdMS_TO_TICKS(2000)); // cooldown between runs
}

// ── Main test task ────────────────────────────────────────────────────
static void _test_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Zemi R&D 2026 — Core Activity 1 Test Harness");
    ESP_LOGI(TAG, "Hypothesis A/B/C — Filter x PSM variant matrix");

    // Init I2C
    ESP_ERROR_CHECK(_i2c_init());
    vTaskDelay(pdMS_TO_TICKS(100));

    // Init sensor
    mlx90393_config_t cfg = MLX90393_DEFAULT_CONFIG();
    mlx90393_dev_t sensor;
    esp_err_t ret = mlx90393_init(&sensor, &cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor init failed! Check wiring. Halting.");
        vTaskDelete(NULL);
    }

    // Print CSV header
    printf("\n--- ZEMI R&D 2026 - CORE ACTIVITY 1 - RAW EVENT LOG ---\n");
    printf("CSV,run_id,filter,psm_variant,event,state,magnitude_uT,timestamp_ms\n");

    // ── Experiment matrix: 4 filters × 3 PSM variants = 12 runs ─────
    zemi_filter_type_t filters[] = {
        ZEMI_FILTER_NONE,
        ZEMI_FILTER_MOVING_AVG,
        ZEMI_FILTER_KALMAN,
        ZEMI_FILTER_HYBRID,
    };
    psm_variant_t variants[] = {
        PSM_VARIANT_V1_SIMPLE,
        PSM_VARIANT_V2_DWELL,
        PSM_VARIANT_V3_DIRECTIONAL,
    };

    for (int f = 0; f < 4; f++) {
        for (int v = 0; v < 3; v++) {
            _run_experiment(&sensor, filters[f], variants[v]);
        }
    }

    printf("\n--- ALL RUNS COMPLETE ---\n");
    ESP_LOGI(TAG, "Test matrix complete. Copy CSV output for analysis.");

    // Idle — don't delete so serial output is still readable
    while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

void app_main(void)
{
    xTaskCreatePinnedToCore(
        _test_task,
        "zemi_test",
        8192,
        NULL,
        5,
        NULL,
        1   // Core 1 — keep Core 0 for BT/WiFi
    );
}
