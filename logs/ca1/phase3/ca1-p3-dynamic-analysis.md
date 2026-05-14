# CA1 Dynamic Phase Aggregated Analysis
Date: 2026-05-14
Tag: ca1-p3-dynamic-analysis
Phase: 3 — Scale Testing

## Summary
Aggregated dynamic test results across Days 8-13 (walking, backpack EMI, active play).

## Results
- D+V3.1 dynamic walk: 96.0% success, 0% FP, 218ms mean (Day 8)
- UART geometric failure analysis: Z-axis 61-67% at failures confirmed (Day 9)
- Backpack EMI: 95.5%, spike-gate rejected all buckle EMI (Day 10)
- Active play D+V3.1: 94.5% MARGINAL, 11 dwell timeouts (Day 11)
- D+V3.2 firmware iteration: 94/95 sim pass (Day 12)
- Active play D+V3.2: 96.5%, 214ms mean PASS (Day 13)

## Conclusion
D+V3.2 firmware resolves dynamic mode marginal result. Both Hyp A+B confirmed
across all dynamic test conditions. Phase 3 dynamic testing complete.
