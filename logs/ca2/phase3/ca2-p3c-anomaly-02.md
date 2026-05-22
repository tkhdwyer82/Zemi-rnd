# CA2 Phase 3C — Anomaly Investigation Session 2
Date: 2026-05-23
Tag: ca2-p3c-anomaly-02
Phase: 3C — Anomaly Investigation

## Session 2 — S3 dT/dt Elevated Ambient Investigation
- Reproduced elevated ambient condition: 24.1C (matching session 3 conditions)
- S3 predictive ramp with dT/dt window 35s (post-patch)
- 4 x 45min thermal characterisation runs
- dT/dt response times: 34.2s, 33.8s, 34.5s, 34.1s at 24.1C ambient
- Compared to 21C baseline: 31.4s, 31.9s, 31.6s, 31.8s
- Delta at elevated ambient: +2.4s average — within 35s window design margin

## Root Cause
Elevated ambient reduces available thermal headroom, causing S3 to activate
ramp earlier. Behaviour is by design — dT/dt window extended to 35s specifically
to accommodate this. No firmware defect.

## Status: ANOMALY 2 RESOLVED — behaviour within design parameters
## Phase 3C anomaly investigation complete — all anomalies resolved
## Phase 3 fully signed off — proceeding to Phase 4
