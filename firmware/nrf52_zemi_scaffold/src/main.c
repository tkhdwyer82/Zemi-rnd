/**
 * @file main.c
 * @brief Zemi nRF52 Wearable — Firmware Orchestration Entry Point
 *
 * Zemi R&D 2026 — nRF52 Wearable Firmware Scaffold
 * Contractor: Timothy Dwyer (TKD Research and Consulting)
 * Company:    Zemi Pty Ltd
 * Created:    April 2026
 *
 * Architecture
 * ────────────
 * Two FreeRTOS tasks run concurrently:
 *
 *   magnetic_task  (50 Hz)
 *     Reads the MLX90393 in burst mode and feeds samples into the
 *     pairing state machine (V3 directional, Kalman-filtered).
 *     Publishes PAIR_EVENT_* to a queue consumed by the main loop.
 *
 *   gesture_task   (100 Hz)
 *     Reads the MPU-6050 and runs the three gesture detectors.
 *     ALIGN       → triggers psm_start_scanning() if IDLE
 *     SINGLE_WAVE → confirms pairing (UI feedback placeholder)
 *     WRIST_FLICK → calls psm_unpair() and cancels pairing UI
 *
 * Pin assignment (nRF52840-DK defaults, override in board_config.h):
 *   SDA = P0.26,  SCL = P0.27
 *   Both sensors share the same I2C bus (TWIM0).
 *
 * Build
 * ─────
 *   This file is a scaffold — it compiles against the nRF5 SDK headers.
 *   Link with:
 *     - nrfx_twim driver
 *     - FreeRTOS kernel
 *     - nrf_log backend (UART or RTT)
 *     - app_timer (provides zemi_get_tick_ms)
 *   See CMakeLists.txt for the full dependency list.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// nRF5 SDK / nrfx
#include "nrfx_twim.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_delay.h"
#include "app_timer.h"
#include "boards.h"

// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Zemi drivers
#include "mlx90393.h"
#include "mpu6050.h"

// ── Board-level pin configuration ───────────────────────────────────
// Override these in a board_config.h or CMake define for your hardware.
#ifndef ZEMI_I2C_SDA_PIN
#define ZEMI_I2C_SDA_PIN    26
#endif
#ifndef ZEMI_I2C_SCL_PIN
#define ZEMI_I2C_SCL_PIN    27
#endif
#define ZEMI_TWIM_INSTANCE  0           // TWIM0

// ── Task configuration ───────────────────────────────────────────────
#define MAGNETIC_TASK_STACK_DEPTH   512     // words
#define GESTURE_TASK_STACK_DEPTH    512
#define MAGNETIC_TASK_PRIORITY      2
#define GESTURE_TASK_PRIORITY       2

#define MAGNETIC_SAMPLE_INTERVAL_MS 20      // 50 Hz
#define GESTURE_SAMPLE_INTERVAL_MS  10      // 100 Hz

// ── Event queue ──────────────────────────────────────────────────────
// The magnetic task posts pair_event_t values; the idle loop consumes them.
#define PAIR_EVENT_QUEUE_LENGTH     8

typedef struct {
    pair_event_t event;
    float        magnitude_uT;
    uint32_t     timestamp_ms;
} pair_event_msg_t;

// ── Shared state ─────────────────────────────────────────────────────
// Pairing state machine is written by magnetic_task only;
// gesture_task calls psm_start_scanning / psm_unpair under portMUX.
static psm_handle_t  s_psm;
static portMUX_TYPE  s_psm_mux = portMUX_INITIALIZER_UNLOCKED;

static QueueHandle_t s_pair_event_queue;

// ── nrfx TWIM instance ───────────────────────────────────────────────
static const nrfx_twim_t s_twim = NRFX_TWIM_INSTANCE(ZEMI_TWIM_INSTANCE);

// ── Device handles ───────────────────────────────────────────────────
static mlx90393_dev_t s_mag;
static mpu6050_dev_t  s_imu;

// ── Millisecond tick (satisfies weak symbol in drivers) ──────────────
uint32_t zemi_get_tick_ms(void)
{
    return (uint32_t)(app_timer_cnt_get() * 1000 / APP_TIMER_CLOCK_FREQ);
}

// ── PSM event callback (called from magnetic_task context) ──────────
static void on_pair_event(pair_event_t event,
                           const psm_handle_t *handle,
                           void *user_data)
{
    (void)user_data;
    pair_event_msg_t msg = {
        .event        = event,
        .magnitude_uT = handle->last_magnitude_uT,
        .timestamp_ms = zemi_get_tick_ms(),
    };
    // Non-blocking post; drop if queue full (task will log the miss)
    xQueueSendFromISR(s_pair_event_queue, &msg, NULL);
}

// ── Magnetic sensor task (50 Hz) ────────────────────────────────────
static void magnetic_task(void *arg)
{
    (void)arg;
    NRF_LOG_INFO("[MAG] task started");

    ret_code_t err = mlx90393_start_burst(&s_mag, MLX90393_AXIS_XYZ);
    if (err != NRF_SUCCESS) {
        NRF_LOG_ERROR("[MAG] burst start failed err=%d — task exiting", err);
        vTaskDelete(NULL);
        return;
    }

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        mlx90393_measurement_t meas;
        err = mlx90393_read_burst(&s_mag, &meas);
        if (err == NRF_SUCCESS && meas.valid) {
            taskENTER_CRITICAL();
            psm_update(&s_psm, &meas, on_pair_event, NULL);
            taskEXIT_CRITICAL();
        } else {
            NRF_LOG_WARNING("[MAG] read error or invalid sample err=%d", err);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(MAGNETIC_SAMPLE_INTERVAL_MS));
    }
}

// ── Gesture / IMU task (100 Hz) ──────────────────────────────────────
static void gesture_task(void *arg)
{
    (void)arg;
    NRF_LOG_INFO("[IMU] task started");

    gesture_detector_t detector;
    gesture_detector_init(&detector);

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        mpu6050_data_t imu;
        ret_code_t err = mpu6050_read(&s_imu, &imu);
        if (err != NRF_SUCCESS || !imu.valid) {
            NRF_LOG_WARNING("[IMU] read error err=%d", err);
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(GESTURE_SAMPLE_INTERVAL_MS));
            continue;
        }

        uint32_t now_ms = zemi_get_tick_ms();
        zemi_gesture_t gesture = mpu6050_detect_gesture(&detector, &imu, now_ms);

        if (gesture != GESTURE_NONE) {
            NRF_LOG_INFO("[IMU] gesture=%s", gesture_name(gesture));
        }

        switch (gesture) {
            case GESTURE_ALIGN:
                // ALIGN signals intent to pair — start scanning if idle
                taskENTER_CRITICAL();
                if (psm_get_state(&s_psm) == PAIR_STATE_IDLE) {
                    psm_start_scanning(&s_psm);
                    NRF_LOG_INFO("[ORCH] ALIGN → start scanning");
                }
                taskEXIT_CRITICAL();
                break;

            case GESTURE_SINGLE_WAVE:
                // WAVE while paired = confirm action (placeholder for UI/BLE event)
                taskENTER_CRITICAL();
                if (psm_get_state(&s_psm) == PAIR_STATE_PAIRED) {
                    NRF_LOG_INFO("[ORCH] WAVE → confirm action (paired)");
                    // TODO: post BLE notification or haptic trigger
                }
                taskEXIT_CRITICAL();
                break;

            case GESTURE_WRIST_FLICK:
                // FLICK = reject / cancel pairing
                taskENTER_CRITICAL();
                {
                    pair_state_t st = psm_get_state(&s_psm);
                    if (st == PAIR_STATE_SCANNING   ||
                        st == PAIR_STATE_DETECTING  ||
                        st == PAIR_STATE_PAIRED) {
                        psm_unpair(&s_psm);
                        NRF_LOG_INFO("[ORCH] FLICK → unpaired");
                    }
                }
                taskEXIT_CRITICAL();
                break;

            default:
                break;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(GESTURE_SAMPLE_INTERVAL_MS));
    }
}

// ── Pair event consumer (runs in main/idle context) ──────────────────
static void process_pair_events(void)
{
    pair_event_msg_t msg;
    while (xQueueReceive(s_pair_event_queue, &msg, 0) == pdTRUE) {
        NRF_LOG_INFO("[PAIR] event=%d mag=%.1fuT t=%ums",
                     msg.event, msg.magnitude_uT, msg.timestamp_ms);

        switch (msg.event) {
            case PAIR_EVENT_PAIRED:
                NRF_LOG_INFO("[PAIR] *** PAIRED *** mag=%.1fuT", msg.magnitude_uT);
                // TODO: start BLE pairing handshake
                break;

            case PAIR_EVENT_UNPAIRED:
                NRF_LOG_INFO("[PAIR] Device out of range — reconnecting");
                break;

            case PAIR_EVENT_RECONNECT_OK:
                NRF_LOG_INFO("[PAIR] Reconnected");
                break;

            case PAIR_EVENT_TIMEOUT:
                NRF_LOG_WARNING("[PAIR] Pairing timeout — returning to IDLE");
                taskENTER_CRITICAL();
                psm_unpair(&s_psm);
                taskEXIT_CRITICAL();
                break;

            default:
                break;
        }
    }
}

// ── Hardware initialisation ──────────────────────────────────────────
static ret_code_t init_twim(void)
{
    nrfx_twim_config_t cfg = NRFX_TWIM_DEFAULT_CONFIG;
    cfg.scl                = ZEMI_I2C_SCL_PIN;
    cfg.sda                = ZEMI_I2C_SDA_PIN;
    cfg.frequency          = NRF_TWIM_FREQ_400K;
    cfg.interrupt_priority = APP_IRQ_PRIORITY_HIGH;
    cfg.hold_bus_uninit    = false;

    ret_code_t err = nrfx_twim_init(&s_twim, &cfg, NULL, NULL);
    if (err != NRF_SUCCESS) return err;

    nrfx_twim_enable(&s_twim);
    NRF_LOG_INFO("TWIM0 enabled SDA=%d SCL=%d", ZEMI_I2C_SDA_PIN, ZEMI_I2C_SCL_PIN);
    return NRF_SUCCESS;
}

static ret_code_t init_sensors(void)
{
    ret_code_t err;

    // ── MLX90393 ──
    mlx90393_config_t mag_cfg = MLX90393_DEFAULT_CONFIG(&s_twim);
    err = mlx90393_init(&s_mag, &mag_cfg);
    if (err != NRF_SUCCESS) {
        NRF_LOG_ERROR("MLX90393 init failed err=%d", err);
        return err;
    }

    // ── MPU-6050 ──
    mpu6050_config_t imu_cfg = MPU6050_DEFAULT_CONFIG(&s_twim);
    err = mpu6050_init(&s_imu, &imu_cfg);
    if (err != NRF_SUCCESS) {
        NRF_LOG_ERROR("MPU6050 init failed err=%d", err);
        return err;
    }

    return NRF_SUCCESS;
}

static void init_pairing_state_machine(void)
{
    // V3 directional + Kalman filter — best balance from CA1 experiments
    psm_init(&s_psm,
             PSM_VARIANT_V3_DIRECTIONAL,
             ZEMI_FILTER_KALMAN,
             on_pair_event,
             NULL);
    NRF_LOG_INFO("PSM initialised (V3_DIRECTIONAL + KALMAN)");
}

// ── Application entry ────────────────────────────────────────────────
int main(void)
{
    ret_code_t err;

    // Logging
    err = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    NRF_LOG_INFO("Zemi nRF52 firmware scaffold — booting");

    // App timer (provides zemi_get_tick_ms)
    err = app_timer_init();
    APP_ERROR_CHECK(err);

    // I2C bus
    err = init_twim();
    APP_ERROR_CHECK(err);

    // Sensors
    err = init_sensors();
    APP_ERROR_CHECK(err);

    // Pairing state machine
    init_pairing_state_machine();

    // Event queue
    s_pair_event_queue = xQueueCreate(PAIR_EVENT_QUEUE_LENGTH,
                                       sizeof(pair_event_msg_t));
    if (!s_pair_event_queue) {
        NRF_LOG_ERROR("Failed to create event queue");
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
    }

    // FreeRTOS tasks
    BaseType_t ok;
    ok = xTaskCreate(magnetic_task, "MAG",
                     MAGNETIC_TASK_STACK_DEPTH, NULL,
                     MAGNETIC_TASK_PRIORITY, NULL);
    APP_ERROR_CHECK_BOOL(ok == pdPASS);

    ok = xTaskCreate(gesture_task, "IMU",
                     GESTURE_TASK_STACK_DEPTH, NULL,
                     GESTURE_TASK_PRIORITY, NULL);
    APP_ERROR_CHECK_BOOL(ok == pdPASS);

    NRF_LOG_INFO("Starting FreeRTOS scheduler");
    NRF_LOG_FLUSH();

    vTaskStartScheduler();

    // Should never reach here
    for (;;) {
        process_pair_events();  // fallback if scheduler not used
        NRF_LOG_FLUSH();
    }
}
