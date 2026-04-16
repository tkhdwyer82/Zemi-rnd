# Zemi R&D 2026 — Core Activity 1: 3D Magnetic Sensor Firmware

**Contractor:** Timothy Dwyer (TKD Research and Consulting, ABN: 670 960 586)  
**Company:** Zemi Pty Ltd  
**Target MCU:** ESP32-S3 (Zemi wearable prototype)  
**Sensor:** Melexis MLX90393 — 3-axis Hall-effect magnetic field sensor  
**Build System:** ESP-IDF v5.x  

---

## Research Objective

Can the MLX90393 3D magnetic sensor be integrated into the Zemi wearable to achieve **≥95% pairing success rate** across 0–1m with **<2% false-positive pairing events** in multi-device environments (school/playground)?

---

## Firmware Architecture

```
core_activity_1_magnetic_sensor/
├── include/
│   ├── mlx90393.h                  # Sensor driver API
│   ├── noise_filter.h              # Filter iterations A–D
│   └── pairing_state_machine.h     # PSM variants V1–V3
├── src/
│   ├── mlx90393.c                  # MLX90393 I2C driver (ESP32-S3)
│   ├── noise_filter.c              # All noise filter implementations
│   └── pairing_state_machine.c     # All state machine variants
├── tests/
│   └── test_sensor_firmware.c      # R&D test harness (4×3 matrix)
├── docs/
│   └── experiment_log.md           # R&D experiment log (ATO substantiation)
└── CMakeLists.txt
```

---

## Noise Filter Iterations (Experiment 1.1)

| Iteration | Filter Type | Description |
|-----------|-------------|-------------|
| **A** | None (baseline) | FY25 firmware — no filtering. Benchmark. |
| **B** | Moving Average | 8-sample window per axis. Tunable window size. |
| **C** | Kalman Filter | 1D Kalman per axis. Q/R tuned to Gen Alpha movement signatures from FY25 data. |
| **D** | Hybrid | Spike gate (rejects Δ > 50µT) → Kalman. Optimised for multi-device EMI environments. |

**Key tuning parameters (noise_filter.h):**
- `MOVING_AVG_WINDOW` — increase to smooth more (trades latency)
- `KALMAN_Q_DEFAULT` — process noise. Increase to trust measurements more.
- `KALMAN_R_DEFAULT` — measurement noise. Increase to trust model more.
- `HYBRID_SPIKE_GATE_uT` — rejection threshold for EMI spikes.

---

## Pairing State Machine Variants (Experiment 1.2)

| Variant | Description | Key behaviour |
|---------|-------------|---------------|
| **V1 Simple** | Threshold only | Pair immediately when magnitude > 5µT |
| **V2 Dwell** | Threshold + hold time | Field must hold ≥ 150ms before pairing confirmed |
| **V3 Directional** | Z-axis dominant | Only pairs when Z-axis > 70% of magnitude (face-to-face approach) |

All variants share the same state enum (`IDLE → SCANNING → DETECTING → PAIRED`) and telemetry interface for direct comparison.

---

## Test Harness

`tests/test_sensor_firmware.c` runs a **4 filter × 3 PSM variant = 12-run matrix**.

Each run outputs:
- **CSV event log** (every state transition with timestamp and magnitude)
- **Run summary** (success rate, false positive rate, avg/min/max latency, samples rejected)

### Serial output format

```
CSV,run_id,filter,psm_variant,event,state,magnitude_uT,timestamp_ms
CSV,1,KALMAN (C),V2_DWELL,PAIRED,PAIRED,12.45,4521
...
SUMMARY,run=1,filter=KALMAN (C),variant=V2_DWELL,samples=1500,...
```

Pipe serial output to a `.csv` file for analysis:
```bash
idf.py monitor | tee results/run_$(date +%Y%m%d_%H%M).csv
```

---

## Build & Flash

```bash
# Prerequisites: ESP-IDF v5.x installed and activated
cd firmware/core_activity_1_magnetic_sensor
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## R&D Telemetry Fields (ATO Substantiation)

Each run logs the following metrics for the R&D experiment log:

| Metric | Target (Hypothesis A) | Notes |
|--------|----------------------|-------|
| `success_rate` | ≥ 95% | Primary KPI |
| `fp_rate` | < 2% | False positive rate |
| `latency_avg_ms` | < 200ms | User-perceived responsiveness |
| `filter_rejected` | — | Samples rejected by filter (telemetry only) |
| `reconnects` | — | Successful reconnect count |
| `timeouts` | — | Pairing timeouts (failure mode) |

---

## Versioning & R&D Log

All firmware iterations **must be committed with a meaningful message** referencing the experiment number:

```
git commit -m "Exp 1.1-C: Kalman filter — initial Q=0.01 R=0.5 (Gen Alpha tuned)"
git commit -m "Exp 1.2-V2: Dwell state machine — extend dwell to 200ms per test results"
```

This commit history forms part of the ATO R&D Tax Incentive substantiation record.

---

*Zemi Pty Ltd — Confidential — April 2026*
