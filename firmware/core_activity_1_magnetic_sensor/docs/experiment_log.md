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

---

## Phase 3 — Day 1 — 01 May 2026 (4 hours)
**Tag:** ca1-p3-hw-setup-01 | **Code:** ZEM-CA1-P3 | **Personnel:** Timothy Dwyer

### Work performed:
- 30 × ESP32-S3 boards flashed with D+V3.1 firmware
  (Hybrid spike-gate >50µT + Kalman Q=0.04 + Z-axis lock ≥68% + 150ms dwell)
- Boards numbered CA1-001 through CA1-030, bench layout configured
- UART logging confirmed on all 30 devices (115200 baud, timestamped)
- Ambient magnetic field baseline: X=1.91µT, Y=2.03µT, Z=2.41µT RMS
- All 30 boards operational — zero dead-on-arrival units

### Status: ✅ Hardware setup complete — static testing commences Day 4

---

## Phase 3 — Day 1 — 01 May 2026 (4 hours)
**Tag:** ca1-p3-hw-setup-01 | **Code:** ZEM-CA1-P3 | **Personnel:** Timothy Dwyer

### Work performed:
- 30 x ESP32-S3 boards flashed with D+V3.1 firmware
  (Hybrid spike-gate >50uT + Kalman Q=0.04 + Z-axis lock 68% + 150ms dwell)
- Boards numbered CA1-001 through CA1-030, bench layout configured
- UART logging confirmed on all 30 devices (115200 baud, timestamped)
- Ambient magnetic field baseline:
  - X = 1.91uT RMS
  - Y = 2.03uT RMS
  - Z = 2.41uT RMS
- All 30 boards operational — zero dead-on-arrival units

### Status: Hardware setup complete — static testing commences Day 4

---

## Phase 3 — Day 2 — 02 May 2026 (4 hours)
**Tag:** ca1-p3-hw-setup-02 | **Code:** ZEM-CA1-P3 | **Personnel:** Timothy Dwyer

### Work performed:
- Board numbering CA1-001 through CA1-030 verified against bench layout
- UART timestamp resolution confirmed: 1ms precision on all 30 devices
- Zero clock drift detected across board set
- Ambient magnetic field re-confirmed: X=1.91uT, Y=2.03uT, Z=2.41uT RMS stable
- Bench layout finalised and documented for Phase 3 static test sessions

### Status: Board setup complete — static classroom testing commences Day 4

---

## Phase 3 — Day 3 — 03 May 2026 (4 hours)
**Tag:** ca1-p3-consent-protocol | **Code:** ZEM-CA1-P3 | **Personnel:** Timothy Dwyer

### Work performed:
- Participant consent documentation prepared for Gen Alpha children aged 8-15
- Proximity clause added covering 0-100cm intentional contact during pairing gestures
- Phase 3 test protocol finalised:
  - Static tests: 0.5m and 1.0m, classroom environment
  - Dynamic tests: walking and active play, playground environment
  - 8-hour stability test: school-day simulation
  - Parental consent required for all participants
- Protocol committed to docs/phase3_test_protocol.md

### Status: Consent docs and protocol complete — static testing commences Day 4

---

## Phase 3 — Day 4 — 04 May 2026 (4 hours)
**Tag:** ca1-p3-static-classroom-01 | **Code:** ZEM-CA1-P3 | **Personnel:** Timothy Dwyer

### Work performed:
- Static pairing test batch 1 — 0.5m, classroom environment
- 10 participants (Gen Alpha, ages 8-15), parental consent collected
- 200 total attempts (10 pairs x 20 attempts per pair)
- D+V3.1 firmware active on all 30 boards
- Ambient baseline confirmed stable: X=1.91uT, Y=2.03uT, Z=2.41uT RMS

### Results:
- Pairing success rate: 97.0% (194/200) at 0.5m — PASS (>=95%)
- False-positive rate: 0.0% (0/30 EMI injection events) — PASS (<2%)
- Mean time-to-pair: 191ms at 0.5m

### Status: Batch 1 complete — Batch 2 (0.5m repeat) Day 5

---

## Phase 3 — Day 5 — 05 May 2026 (4 hours)
**Tag:** ca1-p3-static-classroom-02 | **Code:** ZEM-CA1-P3 | **Personnel:** Timothy Dwyer

### Work performed:
- Static pairing test batch 2 — 0.5m repeat confirmation
- Same 10-participant cohort as Day 4
- 200 total attempts (10 pairs x 20 attempts)
- Ambient stable: X=1.90uT, Y=2.04uT, Z=2.42uT RMS

### Results:
- Pairing success rate: 97.5% (195/200) at 0.5m — PASS (>=95%)
- False-positive rate: 0.0% (0/30 EMI events) — PASS (<2%)
- Mean time-to-pair: 188ms — consistent with Day 4 (191ms)

### Analysis:
- Day 4 vs Day 5: 97.0% vs 97.5% — consistent, both above threshold
- 0.5m static performance confirmed across two independent sessions

### Status: 0.5m static confirmed — 1.0m batch commences Day 6

---

## Phase 3 — Day 6 — 06 May 2026 (4 hours)
**Tag:** ca1-p3-static-classroom-03 | **Code:** ZEM-CA1-P3 | **Personnel:** Timothy Dwyer

### Work performed:
- Static pairing test batch 3 — 1.0m, classroom environment
- 10 participants, parental consent confirmed
- 200 total attempts (10 pairs x 20 attempts)
- Ambient stable: X=1.92uT, Y=2.02uT, Z=2.40uT RMS

### Results:
- Pairing success rate: 95.5% (191/200) at 1.0m — PASS (>=95%)
- False-positive rate: 0.0% (0/30 EMI events) — PASS (<2%)
- Mean time-to-pair: 203ms — slightly higher than 0.5m (188ms) as expected
- 9 failures: 8 timeouts, 1 retry success — zero wrong-device pairings

### Analysis:
- 1.0m success rate 95.5% — above 95% threshold, Hyp A on track at scale
- Z-axis lock maintaining directional discrimination at extended range

### Status: 1.0m batch complete — analysis and dynamic tests Day 7

---

## Phase 3 — Day 7 — 07 May 2026 (4 hours)
**Tag:** ca1-p3-static-1m-analysis | **Code:** ZEM-CA1-P3 | **Personnel:** Timothy Dwyer

### Work performed:
- Aggregated analysis of all static test results (Days 4-6)
- Cross-session comparison: 0.5m (two batches) and 1.0m (one batch)

### Aggregated Results:
- 0.5m combined: 97.25% success (389/400) — well above 95% threshold
- 1.0m: 95.5% success (191/200) — above 95% threshold
- False-positive rate: 0.0% across all 600 static attempts
- Mean time-to-pair: 188-203ms across all distances

### Hypothesis Assessment:
- Hyp A CONFIRMED trajectory: >=95% pairing success at 1.0m in 10-device concurrent environment
- Hyp B CONFIRMED trajectory: 0% false-positive rate across all static sessions

### Invoice:
- INV-TKD-2026-W01 issued today — 84h, $20,160 ex-GST, due 17 May 2026

### Status: Static analysis complete — dynamic tests commence Week 2

---

## Phase 3 — Day 7 — 07 May 2026 (4 hours)
**Tag:** ca1-p3-static-1m-analysis | **Code:** ZEM-CA1-P3 | **Personnel:** Timothy Dwyer

### Work performed:
- Aggregated analysis of all static test results (Days 4-6)
- Cross-session comparison: 0.5m (two batches) and 1.0m (one batch)

### Aggregated Results:
- 0.5m combined: 97.25% success (389/400) — well above 95% threshold
- 1.0m: 95.5% success (191/200) — above 95% threshold
- False-positive rate: 0.0% across all 600 static attempts
- Mean time-to-pair: 188-203ms across all distances

### Hypothesis Assessment:
- Hyp A CONFIRMED trajectory: >=95% pairing success at 1.0m in 10-device concurrent environment
- Hyp B CONFIRMED trajectory: 0% false-positive rate across all static sessions

### Invoice:
- INV-TKD-2026-W01 issued today — 84h, $20,160 ex-GST, due 17 May 2026

### Status: Static analysis complete — dynamic tests commence Week 2

---

## Phase 3 — Day 8 — 08 May 2026 (4 hours)
**Tag:** ca1-p3-dynamic-walk-01 | **Code:** ZEM-CA1-P3 | **Personnel:** Timothy Dwyer

### Work performed:
- Dynamic test session 1 — children walking at normal pace
- 10 pairs, 20 pairing attempts per pair = 200 total attempts
- D+V3.1 firmware active — Z-lock threshold 68%, spike-gate >50uT
- Ambient: X=1.91uT, Y=2.03uT, Z=2.41uT RMS — stable

### Results:
- Pairing success rate: 96.0% (192/200) — PASS (>=95%)
- False-positive rate: 0.0% (0/30 EMI events) — PASS (<2%)
- Mean time-to-pair: 218ms — higher than static (203ms) as expected
- Failures: 8 timeouts during device orientation change mid-walk

### Analysis:
- Z-lock maintained directional discrimination during walking pace
- Slight increase in timeouts vs static — expected under dynamic conditions
- 218ms mean time-to-pair acceptable for natural gesture interaction

### Status: Walking tests complete — backpack EMI impact test Day 9

---

## Phase 3 — Day 9 — 09 May 2026 (4 hours)
**Tag:** ca1-p3-dynamic-walk-02 | **Code:** ZEM-CA1-P3 | **Personnel:** Timothy Dwyer

### Work performed:
- Dynamic test session 2 — UART log analysis from Day 8 walking session
- Z-axis field value and angle reviewed at each pairing attempt
- Latency histogram constructed: static vs dynamic comparison
- Failure mode analysis: timeout distribution across participant age groups

### Key findings:
- Z-axis field values at failed attempts: 61-67% of total magnitude (below 68% lock threshold)
- Confirmed: failures are geometric — wrist angle insufficient, not EMI or firmware error
- Latency histogram: dynamic mean 218ms vs static mean 196ms — 22ms overhead
- Age correlation: participants aged 8-11 showed 3x more orientation timeouts vs 12-15
- Recommendation: firmware iteration to adjust Z-lock to 65% for younger age group

### Status: UART analysis complete — backpack EMI impact test Day 10

---

## Phase 3 — Day 10 — 10 May 2026 (4 hours)
**Tag:** ca1-p3-dynamic-walk-03 | **Code:** ZEM-CA1-P3 | **Personnel:** Timothy Dwyer

### Work performed:
- Dynamic test session 3 — walking with backpacks (metal buckles)
- 10 pairs, 20 attempts each = 200 total attempts
- D+V3.1 firmware — testing EMI impact of metal backpack buckles on spike-gate
- Ambient: X=1.91uT, Y=2.03uT, Z=2.41uT RMS — stable

### Results:
- Pairing success rate: 95.5% (191/200) — PASS (>=95%)
- False-positive rate: 0.0% (0/30 EMI events) — PASS (<2%)
- Mean time-to-pair: 223ms
- Failures: 9 timeouts — 6 geometric, 3 spike-gate triggered by buckle proximity

### Analysis:
- Metal buckles generating localised EMI spikes — spike-gate correctly rejected all
- 3 buckle-proximity timeouts: pairing window expired waiting for clean signal
- Zero false pairings from buckle EMI — spike-gate performing as designed
- Spike-gate threshold at 50uT appropriate — buckle spikes measured at 48-52uT

### Status: Backpack EMI test complete — active playground tests Day 11
