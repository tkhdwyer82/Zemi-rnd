# Zemi R&D — tkhdwyer82/Zemi-rnd

**Company:** Zemi Pty Ltd | **Contractor:** Timothy Dwyer (TKD Research & Consulting)
**Program:** FY2026 R&D — Smart Wearable for Generation Alpha
**ATO:** Australian R&D Tax Incentive (Section 355, ITAA 1997)

---

## Repository Structure

```
firmware/
├── core_activity_1_magnetic_sensor/
│   ├── src/
│   │   ├── mlx90393.c / .h              — MLX90393 I2C driver (ESP32-S3)
│   │   ├── noise_filter.c / .h          — Filter iterations A (baseline), B (moving avg),
│   │   │                                   C (Kalman, Gen Alpha Q/R), D (hybrid spike+Kalman)
│   │   └── pairing_state_machine.c / .h — PSM variants V1 (threshold), V2 (dwell), V3 (Z-lock)
│   ├── tests/
│   │   └── test_sensor_firmware.c       — 4×3 filter × PSM test matrix harness
│   └── docs/
│       └── experiment_log.md            — ATO R&D substantiation log (Exp 1.1, 1.2)
│
└── core_activity_2_inductive_charging/
    ├── src/
    │   ├── thermal_manager.c / .h       — Thermal strategies S1 (fixed), S2 (PID), S3 (predictive)
    │   ├── flexible_ferrite.c / .h      — Ferrite configs M1 (rigid), M2 (flexible), M3 (segmented)
    │   └── ble_coexistence.c / .h       — BLE modes X1 (continuous), X2 (burst), X3 (charge-gated)
    ├── tests/
    │   └── test_charging_firmware.c     — CA2 test harness (3×3×3 matrix)
    └── docs/
        └── experiment_log.md            — ATO R&D substantiation log (Exp 2.1, 2.2, 2.3)
```

## Core Activity 1 — 3D Magnetic Sensor Production-Readiness

**Research Question:** Can the MLX90393 3D magnetic sensor be production-ready in a
child-safe, durable Zemi wearable at acceptable unit cost and performance?

**Key Unknowns:** Multi-device interference (10-device classroom/playground);
EMI in real-world environments; firmware filter optimal for Gen Alpha motor patterns.

**Experiments:**
- **1.1** Noise filter iterations A→D — target <2% false-positive rate
- **1.2** Pairing state machine variants V1→V3 — target ≥95% success at 1m

**Git Tags (per iteration):**
- `ca1-exp1.1-iteration-a` through `ca1-exp1.1-iteration-d`
- `ca1-exp1.2-v1-simple`, `ca1-exp1.2-v2-dwell`, `ca1-exp1.2-v3-zlock`

## Core Activity 2 — Inductive Charging Solution Completion

**Research Question:** Can the Zemi charging module achieve thermal safety (≤38°C),
flexible ferrite shielding, and ≥70% efficiency simultaneously in wrist-worn geometry?

**FY25 Gap:** FY25 module was ~90% complete — thermal management, flexible ferrite,
and BLE integration were explicitly unresolved in the FY25 Innercode submission.

**Experiments:**
- **2.1** Thermal management strategies S1→S3 — target ≤38°C + ≥70% efficiency
- **2.2** Flexible ferrite material configurations M1→M3 — target ≥70% curved efficiency
- **2.3** BLE coexistence modes X1→X3 — target ≤±3% efficiency delta

**Git Tags (per configuration):**
- `ca2-exp2.1-s1-fixed`, `ca2-exp2.1-s2-pid`, `ca2-exp2.1-s3-predictive`
- `ca2-exp2.2-m1-rigid`, `ca2-exp2.2-m2-flexible`, `ca2-exp2.2-m3-segmented`
- `ca2-exp2.3-x1-continuous`, `ca2-exp2.3-x2-burst`, `ca2-exp2.3-x3-gated`

## Hardware Platform

- MCU: ESP32-S3 (dual-core 240MHz, BLE 5.0, I2C, SPI)
- Magnetic sensor: Melexis MLX90393 (3-axis Hall effect, I2C, 16-bit resolution)
- Charging TX IC: TI BQ500212A (125kHz resonant wireless power transmitter)
- Charging RX IC: TI BQ51050B (Qi receiver, rectification + regulation)
- Thermal sensor: NTC thermistor (10kΩ at 25°C, ADC-read via ESP32-S3)
- Display: 1.3" AMOLED 294×126px (SPI) — hardware reference for CA3 supporting activity

## ATO Compliance

All firmware is versioned and tagged in this repository. Commit history, branch
dates, and tags form part of the ATO R&D Tax Incentive substantiation record for
Zemi Pty Ltd (FY2026). See each `docs/experiment_log.md` for full substantiation
documentation including hypothesis, variables, measurement protocol, and results.

**Contractor:** Timothy Dwyer | TKD Research & Consulting | ABN: 670 960 586
**Company:** Zemi Pty Ltd | Director: Nicole Annette Parkinson
**Confidential — Not for distribution.**
