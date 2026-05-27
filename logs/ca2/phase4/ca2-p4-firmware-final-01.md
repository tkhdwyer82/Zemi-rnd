# CA2 Phase 4 — Production Firmware Finalisation Session 1
Date: 2026-05-28
Tag: ca2-p4-firmware-final-01
Phase: 4 — Production Review

## CA2 Production Firmware Scope
Components: TI BQ500212A (TX) + TI BQ51050B (RX) + ESP32-S3 thermal control

## Firmware Audit and Lock
- S3 predictive ramp: dT/dt window 35s — locked
- X3 charge-gated zero-crossing: gate timing confirmed — locked
- FOD threshold: BQ500212A factory default + 10% margin — locked
- NTC cutoff: 39.5C hardware cutoff (above 38C firmware limit) — confirmed
- BLE coexistence mode: X3 only — continuous and burst modes disabled
- Charging status BLE notification: 500ms interval — confirmed

## Regression Test (3 units, S3+M2+X3)
- Efficiency: 72.3% average — consistent with Phase 3
- Peak temp: 36.8C — within 38C limit
- BLE PDR: 98.3% — above 95% threshold
- X3 delta: -0.4% — within spec

## Status: CA2 production firmware LOCKED — regression PASS
