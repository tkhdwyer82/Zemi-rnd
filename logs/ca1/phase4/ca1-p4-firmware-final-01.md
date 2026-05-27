# CA1 Phase 4 — Production Firmware Finalisation Session 1
Date: 2026-05-27
Tag: ca1-p4-firmware-final-01
Phase: 4 — Production Review

## Production Firmware: D+V3.2 Finalisation
Target: lock firmware for production — no further parameter changes post sign-off

## Session 1 — Firmware Audit
- Noise filter: Hybrid spike-gate >50uT + Kalman — parameters locked
- Pairing state machine: V3.2 — dwell 100ms dynamic / 150ms static, motion flag >15 deg/s
- Z-axis directional lock: >=68% Z-contribution threshold — confirmed
- UART logging: production mode — reduced verbosity, error logging only
- BLE advertisement interval: 100ms — confirmed
- Power management: deep sleep between pairing attempts — confirmed

## Items Remaining
- Final UART log format review for ATO documentation package
- OTA update flag: disable for production units
- Firmware version string: set to D+V3.2-PROD

## Status: Firmware audit 80% complete
