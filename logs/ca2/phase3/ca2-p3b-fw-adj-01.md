# CA2 S3 Firmware Patch — dT/dt Window Extended
Date: 2026-05-14
Tag: ca2-p3b-fw-adj-01
Phase: 3B — Integrated Testing

## Adjustment
S3 predictive ramp dT/dt window extended: 30s -> 35s
Rationale: Session 3 elevated ambient (24.1C) showed tighter thermal headroom.

## Validation
- Ambient: 24.1C
- Peak surface temp: 37.0C (within 38C limit)
- Efficiency: 71.9% (above 70% threshold)
- X3 gate timing unaffected by patch
- BLE PDR: 98.2%

## Status: PASS
