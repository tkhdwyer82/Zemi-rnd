# Experiment Log — Core Activity 2: Inductive Charging Solution — Completion
# Zemi R&D 2026 | ATO R&D Tax Incentive Substantiation Record
# Repo: tkhdwyer82/Zemi-rnd | Path: firmware/core_activity_2_inductive_charging/docs/experiment_log.md

---

## Research Question
Can a compact inductive charging module for the Zemi wearable achieve validated
thermal safety (≤38°C skin contact), flexible ferrite shielding, and ≥75% charging
efficiency simultaneously in a child-wearable form factor?

## FY25 Baseline
FY25 established three foundational results for the sliding inductive power module:
- Resonant tuning + rigid ferrite: >75% efficiency (flat geometry, 5mm coil distance)
- Flexible PCB coils + compact MCU: >60% efficiency (flat geometry)
- Multi-coil + adaptive alignment: >65% efficiency at 10mm misalignment
FY25 experiment declared ~90% complete. Three elements explicitly unresolved:
1. Real-time thermal management (≤38°C not demonstrated)
2. Flexible ferrite shielding in curved wrist geometry (not tested)
3. BLE charging status integration (not implemented)

---

## Experiment 2.1 — Thermal Management Strategies

### Hypothesis
Integrating thermal sensors with firmware-controlled power throttling will maintain
device surface temperature ≤38°C during a continuous 2-hour charging session,
without reducing charging efficiency below 70%.

### Variables
| Variable | Type | Values Under Test |
|---|---|---|
| Throttling strategy | Primary (categorical) | S1 (fixed ceiling), S2 (PID), S3 (predictive) |
| Temperature setpoint | Secondary — S2 | 35°C, 36°C (default), 37°C |
| PID gains | Secondary — S2 | Kp: 3/5/8, Ki: 0.05/0.1/0.2, Kd: 1/2/4 |
| Ramp lookahead | Secondary — S3 | 10s, 20s (default), 30s |
| Power floor | Secondary — all | 40%, 60% (S1 default), 80% |

### Measurement Protocol
- 2-hour continuous charging session per strategy
- IR thermometer readings at wrist contact surface (5 measurement points)
- Charging efficiency = output power (mW) / input power (mW) × 100%
- Temperature logged at 1Hz via firmware serial output
- Each strategy run minimum 3 times; results averaged

### Results (to be populated during Phase 2 lab testing)

| Strategy | Peak Temp (°C) | Mean Temp (°C) | Mean Efficiency (%) | Time >38°C (s) | Pass |
|---|---|---|---|---|---|
| S1 — Fixed Ceiling (60%) | TBD | TBD | TBD | TBD | TBD |
| S2 — PID (SP=36°C) | TBD | TBD | TBD | TBD | TBD |
| S3 — Predictive (LA=20s) | TBD | TBD | TBD | TBD | TBD |

### Firmware References
- src/thermal_manager.c — Strategies S1, S2, S3

### Git Tags
- `ca2-exp2.1-s1-fixed`      — Fixed ceiling strategy
- `ca2-exp2.1-s2-pid`        — PID controller strategy
- `ca2-exp2.1-s3-predictive` — Predictive ramp strategy

---

## Experiment 2.2 — Flexible Ferrite Material Configurations

### Hypothesis
Flexible ferrite composite material will maintain EMI shielding effectiveness
(field attenuation ≥ rigid baseline) while allowing the charging module to conform
to a curved wristband form factor without efficiency loss below 70%.

### Variables
| Variable | Type | Values Under Test |
|---|---|---|
| Ferrite material | Primary (categorical) | M1 (rigid baseline), M2 (flexible), M3 (segmented) |
| Curvature radius | Secondary | 35mm, 40mm (Zemi wrist), 50mm |
| Coil distance | Secondary | 0mm, 5mm (default), 10mm |
| Operating frequency | Secondary | 80kHz (FY25 flexible), 125kHz (FY25 rigid) |

### Measurement Protocol
- Network analyser: measure S21 insertion loss (= shielding attenuation, dB)
- Power meter: measure efficiency at 5mm coil distance, 40mm curvature radius
- IR thermometer: peak surface temperature during 30-min session
- EMI probe at 50mm distance: leakage field (µT) with and without ferrite
- Mechanical durability: 100-cycle bend test at 40mm radius, re-measure efficiency

### Results (to be populated during Phase 2 lab testing)

| Config | Mean Eff (%) | Min Eff (%) | Att (dB) | EMI Leak (µT) | Max Temp (°C) | Hyp-B Pass |
|---|---|---|---|---|---|---|
| M1 — Rigid (FY25 baseline) | TBD | TBD | TBD | TBD | TBD | TBD |
| M2 — Flexible composite | TBD | TBD | TBD | TBD | TBD | TBD |
| M3 — Segmented rigid | TBD | TBD | TBD | TBD | TBD | TBD |

### Firmware References
- src/flexible_ferrite.c — Configuration M1, M2, M3 data acquisition and analysis

### Git Tags
- `ca2-exp2.2-m1-rigid`     — Rigid ferrite baseline
- `ca2-exp2.2-m2-flexible`  — Flexible ferrite composite
- `ca2-exp2.2-m3-segmented` — Segmented tile configuration

---

## Experiment 2.3 — BLE Charging Status Coexistence

### Hypothesis
A firmware-managed BLE charging status module operating at 2.4GHz can coexist
with the 125kHz inductive charging field without measurable degradation in
charging efficiency (within ±3% of no-BLE baseline).

### Variables
| Variable | Type | Values Under Test |
|---|---|---|
| BLE coexistence mode | Primary (categorical) | X1 (continuous), X2 (burst), X3 (charge-gated) |
| BLE advertising interval | Secondary — X1 | 50ms, 100ms (default), 500ms |
| Burst interval | Secondary — X2 | 2s, 5s (default), 10s |
| TX power | Secondary — all | 0dBm, -6dBm (default), -12dBm |

### Measurement Protocol
- Charging efficiency measured with BLE transmitting vs. BLE off (baseline delta)
- MLX90393 noise floor measured with and without BLE TX (RMS µT increase)
- BLE packet delivery ratio: smartphone GATT notification receipt confirmation
- Status notification latency: time from firmware event to app receipt (ms)
- All conditions run simultaneously on same hardware (coexistence = concurrent operation)

### Results (to be populated during Phase 3 integration testing)

| Mode | Eff Delta (%) | MLX90393 Noise Increase (µT RMS) | BLE PDR (%) | Latency (ms) | Pass |
|---|---|---|---|---|---|
| No BLE (baseline) | 0 | 0 | N/A | N/A | Baseline |
| X1 — Continuous (100ms) | TBD | TBD | TBD | TBD | TBD |
| X2 — Burst (5s interval) | TBD | TBD | TBD | TBD | TBD |
| X3 — Charge-Gated | TBD | TBD | TBD | TBD | TBD |

### Firmware References
- src/ble_coexistence.c — Modes X1, X2, X3

### Git Tags
- `ca2-exp2.3-x1-continuous` — Continuous BLE mode
- `ca2-exp2.3-x2-burst`      — Burst BLE mode
- `ca2-exp2.3-x3-gated`      — Charge-gated BLE mode

---

## Success Criteria Summary

| Criterion | Target | Status |
|---|---|---|
| Peak skin temperature | ≤38°C over 2hr session | Pending Exp 2.1 |
| Charging efficiency (thermal constrained) | ≥70% | Pending Exp 2.1 |
| Flexible ferrite efficiency | ≥70% at 5mm, 40mm radius | Pending Exp 2.2 |
| BLE charging efficiency delta | ≤±3% vs no-BLE baseline | Pending Exp 2.3 |
| BLE packet delivery ratio | ≥95% | Pending Exp 2.3 |
| MLX90393 noise increase from BLE | <5µT RMS | Pending Exp 2.3 |

---

## ATO Compliance Notes
- All firmware committed to tkhdwyer82/Zemi-rnd with dated commit history
- Each iteration/configuration tagged in Git prior to test session
- Results entered into this log within 48 hours of each test session
- Timesheets cross-reference this log by experiment code (CA2-2.1, CA2-2.2, CA2-2.3)
- FY25 partial completion documented in Innercode R&D FY25 submission (Core Activity 2)
  providing the prior-year baseline from which FY26 hypotheses are drawn
