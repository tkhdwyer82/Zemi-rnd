# Phase 3 Test Protocol — Core Activity 1
# 3D Magnetic Sensor Production-Readiness
# Date: 2026-05-03 | Tag: ca1-p3-consent-protocol | Personnel: Timothy Dwyer
# ─────────────────────────────────────────────────────────────────────────

## Participant Information
- Age range: 8-15 years (Generation Alpha)
- Parental consent: required for all participants
- Proximity clause: covers 0-100cm intentional device contact during pairing gestures
- Session duration: max 2 hours per participant per day

## Static Test Protocol
- Distance 1: 0.5m — 20 pairing attempts per pair
- Distance 2: 1.0m — 20 pairing attempts per pair
- Environment: classroom (controlled)
- Concurrent devices: 10-30 boards active
- UART logging: continuous throughout session

## Dynamic Test Protocol
- Session A: children walking at normal pace — 20 attempts per pair
- Session B: children with backpacks (metal buckles) — EMI impact assessment
- Session C: active playground play — rapid gesture pairing
- Session D: varied device orientations — angular tolerance assessment

## 8-Hour Stability Protocol
- Devices: 2 units, continuous wear simulation
- Duration: 8 hours (school-day simulation)
- Logging: UART continuous, battery curve recorded
- Pass criteria: zero firmware crashes or resets

## Success Criteria
- Pairing success rate >= 95% at 1.0m (multi-device, static and dynamic)
- False-positive rate < 2% in 30-device concurrent environment
- Zero firmware crashes over 8-hour continuous wear
