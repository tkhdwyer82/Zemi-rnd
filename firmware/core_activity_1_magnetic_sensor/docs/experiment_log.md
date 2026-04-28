# Experiment Log — Core Activity 1: 3D Magnetic Sensor Production-Readiness
# Zemi R&D 2026 | ATO R&D Tax Incentive Substantiation Record
# Repo: tkhdwyer82/Zemi-rnd | Path: firmware/core_activity_1_magnetic_sensor/docs/experiment_log.md

---

## Research Question
Can the MLX90393 3D magnetic sensor be production-ready in a child-safe, durable
Zemi wearable at acceptable unit cost and performance?

## FY25 Baseline
Stable pairing confirmed up to 0.5m in controlled conditions (94% success rate).
At 1m, success rate fell to 90% only in static, single-device conditions.
Multi-device interference, environmental EMI, and miniaturisation remain unresolved.
Knowledge gap: unresolved (carried forward explicitly from FY25 submission).

---

## Experiment 1.1 — Noise Filter Iterations

### Hypothesis
Custom firmware noise-filtering algorithm (iterated from FY25 baseline) will reduce
false-positive pairing events to <2% in a 10-device concurrent test environment.

### Variables
| Variable | Type | Values Under Test |
|---|---|---|
| Filter algorithm | Primary (categorical) | A (baseline), B (moving avg), C (Kalman), D (hybrid) |
| Window size | Secondary — Iteration B | 4, 8 (default), 16 samples |
| Q process noise | Secondary — Iterations C, D | 0.01, 0.05 (Gen Alpha), 0.10 |
| R measurement noise | Secondary — Iterations C, D | 0.1, 0.5 (Gen Alpha), 1.0 |
| Spike gate threshold | Secondary — Iteration D | 20µT, 50µT (default), 100µT |

### Measurement Protocol
- 10-device concurrent test in classroom environment (20 Zemi prototype units,
  10 active transmitters, 10 passive receivers)
- Each condition runs for 30-minute test session
- Pairing events logged to UART and Git commit tagged per iteration
- False positive = pairing event triggered with no intentional pairing gesture
- Success rate = confirmed pairings / intended pairing attempts × 100%

### Results (to be populated during Phase 2 lab testing — Wks 7–12)

| Iteration | Filter Type | Success Rate (%) | False Positive Rate (%) | Pass (≥95% / <2%) |
|---|---|---|---|---|
| A — Baseline | None | TBD | TBD | TBD |
| B — Moving Avg (n=8) | 8-sample window | TBD | TBD | TBD |
| C — Kalman (Q=0.05, R=0.5) | 1D Kalman per axis | TBD | TBD | TBD |
| D — Hybrid (gate=50µT + Kalman) | Spike gate + Kalman | TBD | TBD | TBD |

### Firmware References
- src/noise_filter.c — Iterations A, B, C, D
- src/mlx90393.c    — Sensor I2C driver
- tests/test_sensor_firmware.c — 4×3 test matrix harness

### Git Tags
- `ca1-exp1.1-iteration-a` — Baseline firmware (FY25 carry-forward)
- `ca1-exp1.1-iteration-b` — Moving average filter
- `ca1-exp1.1-iteration-c` — Kalman filter (Gen Alpha Q/R)
- `ca1-exp1.1-iteration-d` — Hybrid spike-gate + Kalman

---

## Experiment 1.2 — Pairing State Machine Variants

### Hypothesis
Miniaturised MLX90393 in TPU/silicone housing with EMI shielding will maintain
≥95% pairing success at 0–1m in real-world multi-device environments.

### Variables
| Variable | Type | Values Under Test |
|---|---|---|
| State machine variant | Primary (categorical) | V1 (threshold), V2 (dwell), V3 (Z-lock) |
| Pairing threshold | Secondary | 3µT, 5µT (default), 8µT |
| Dwell time | Secondary — V2, V3 | 100ms, 150ms (default), 250ms |
| Z-dominance fraction | Secondary — V3 | 60%, 70% (default), 80% |

### Scale Test Conditions
- Phase 3 (Wks 13–18): 10-device concurrent pairing in classroom + playground
- Static conditions: devices held still at 0.25m, 0.5m, 0.75m, 1.0m
- Dynamic conditions: devices worn on wrist during walking and play
- 8-hour wear stability test: 2 devices worn continuously (school-day simulation)

### Results (to be populated during Phase 3 scale testing — Wks 13–18)

| Variant | Architecture | Success Rate @ 1m (%) | False Positives (%) | 8hr Stable | Pass |
|---|---|---|---|---|---|
| V1 — Simple | Threshold only | TBD | TBD | TBD | TBD |
| V2 — Dwell | Threshold + 150ms hold | TBD | TBD | TBD | TBD |
| V3 — Z-Lock | Z-dominant + dwell | TBD | TBD | TBD | TBD |

### Firmware References
- src/pairing_state_machine.c — Variants V1, V2, V3
- tests/test_sensor_firmware.c — 4×3 filter × PSM matrix runner

### Git Tags
- `ca1-exp1.2-v1-simple`  — Simple threshold PSM
- `ca1-exp1.2-v2-dwell`   — Dwell PSM (150ms)
- `ca1-exp1.2-v3-zlock`   — Z-axis dominance lock PSM

---

## Success Criteria Summary

| Criterion | Target | Status |
|---|---|---|
| Pairing success rate at 1m | ≥95% | Pending Phase 3 |
| False-positive rate | <2% | Pending Phase 2 |
| Firmware stable over 8hr | Zero crashes / resets | Pending Phase 3 |
| BOM per-unit cost | Within target range | Pending Phase 4 |

---

## ATO Compliance Notes
- All firmware committed to tkhdwyer82/Zemi-rnd with dated commit history
- Each iteration tagged in Git prior to test session commencement
- Experiment sessions logged with date, personnel (Timothy Dwyer), and hours
- Results entered into this log within 48 hours of each test session
- Timesheets cross-reference this log by experiment code (CA1-1.1, CA1-1.2)
