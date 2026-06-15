# Zemi R&D Firmware Experiment Log - FY2026
**Project:** Zemi – Smart Wearable for Generation Alpha  
**Contractor:** Timothy Dwyer – TKD Research and Consulting  
**Period:** 1 May – 30 June 2026  
**Repo:** https://github.com/tkhdwyer82/Zemi-rnd

This document serves as the master index and summary of all firmware experimentation for Core Activities 1 and 2. All raw UART logs, Git tags, and analysis are linked below.

## Core Activity 1: Experimentation on the Production-Readiness of the 3D Magnetic Field Detector

**Firmware Baseline:** D+V3.2-PROD (Hybrid Spike-Gate + Kalman + Z-Axis Lock)

### Phase 3 Logs - Static & Dynamic Testing

#### Day 4: ca1-p3-static-classroom-01 (Static 0.5m Batch 1)
- **Log:** [ca1-day4-static-classroom-01.log](./ca1-day4-static-classroom-01.log)
- **Results:** 29/30 successful (96.7%) | 0 false positives
- **Notes:** Excellent performance with D+V3.1. Filter D rejected ambient EMI effectively.

#### Day 5: ca1-p3-static-classroom-02 (Static 0.5m Batch 2)
- **Log:** [ca1-day5-static-classroom-02.log](./ca1-day5-static-classroom-02.log)
- **Results:** 39/40 successful (97.5%) | 0 FP

#### Day 6: ca1-p3-static-classroom-03 (Static 1.0m)
- **Log:** [ca1-day6-static-1m.log](./ca1-day6-static-1m.log)
- **Results:** 23/24 successful (95.8%) | 0 FP
- **Notes:** Marginal signal strength at distance handled well by V3 Z-Axis Lock.

#### Day 8: ca1-p3-dynamic-walk-01 (Dynamic Walking)
- **Log:** [ca1-day8-dynamic-walk.log](./ca1-day8-dynamic-walk.log)
- **Results:** 24/25 successful (96.0%) | 0 FP
- **Notes:** MotionFlag (>15 deg/s) active and effective.

#### Other CA1 Logs
- [ca1-raw-sensor-readings.log](./ca1-raw-sensor-readings.log)
- [ca1-noise-filter-comparison.log](./ca1-noise-filter-comparison.log)
- [ca1-multi-device-session.log](./ca1-multi-device-session.log)
- [ca1-false-positive-emi-test.log](./ca1-false-positive-emi-test.log)
- [ca1-dynamic-motion-test.log](./ca1-dynamic-motion-test.log)

**Overall CA1 Outcome:** Hypothesis A & B validated. Production firmware locked as D+V3.2-PROD.

## Core Activity 2: Experimentation on the Completion of the Inductive Magnetic Charging Solution

**Firmware:** S3+M2+X3 (Predictive Ramp + Flexible Ferrite + Charge-Gated BLE)

### Phase 3 Integrated Testing Logs

#### Day 5: ca2-p3b-session1-run (S3+M2+X3 2hr)
- **Log:** [ca2-thermal-management.log](./ca2-thermal-management.log)
- **Results:** Peak 36.6°C | 72.3% efficiency

#### Day 7: ca2-p3b-session2-run (S3+M3+X3)
- **Log:** [ca2-charging-efficiency.log](./ca2-charging-efficiency.log)
- **Results:** Peak 36.4°C | 75.1% efficiency

#### Day 10: ca2-p3b-session3-run (Elevated Ambient 24.1°C)
- **Log:** [ca2-day10-elevated-ambient.log](./ca2-day10-elevated-ambient.log)
- **Results:** Peak 37.1°C | 71.8% efficiency | All criteria met

#### Additional CA2 Logs
- [ca2-ble-coexistence.log](./ca2-ble-coexistence.log)
- [ca2-mlx-noise-during-charge.log](./ca2-mlx-noise-during-charge.log)
- [ca2-ferrite-comparison.log](./ca2-ferrite-comparison.log)
- [ca2-integrated-session.log](./ca2-integrated-session.log)

**Overall CA2 Outcome:** All three hypotheses validated. Production charging firmware locked.

## Supporting Records & Compliance
- Git commit history: 86 commits tagged with `ca1-` and `ca2-` prefixes.
- Compliance: IP67 and AS/NZS 8124 reports in `/docs/compliance/`
- Raw UART logs and timesheets available upon request.

**Last Updated:** 30 June 2026