# Experiment Log — Core Activity 1: 3D Magnetic Sensor
## Zemi R&D 2026

**Contractor:** Timothy Dwyer (TKD Research and Consulting)  
**Company:** Zemi Pty Ltd  
**Activity:** Core Activity 1 — 3D Magnetic Sensor Production-Readiness  
**ATO R&D Tax Incentive:** Section 355, ITAA 1997  

---

## Experiment 1.1 — Noise-Filtering Algorithm Iterations

**Research question:** Which firmware-level noise-filtering approach minimises
false-positive pairing events while maintaining ≥95% true pairing detection
under electromagnetic interference?

**Technical unknowns being resolved:**
1. Whether a moving average filter provides sufficient noise rejection without introducing unacceptable latency
2. Whether a Kalman filter tuned to Gen Alpha motor patterns outperforms generic filtering approaches
3. Whether a hybrid spike-gate approach is necessary for multi-device EMI environments

---

### Run Log

| Run ID | Date | Filter | PSM Variant | Duration | Samples | Success Rate | FP Rate | Avg Latency | Notes |
|--------|------|--------|------------|----------|---------|-------------|---------|-------------|-------|
| 1 | | NONE (A) | V1 | | | | | | Baseline — FY25 equivalent |
| 2 | | NONE (A) | V2 | | | | | | |
| 3 | | NONE (A) | V3 | | | | | | |
| 4 | | MOVING_AVG (B) | V1 | | | | | | |
| 5 | | MOVING_AVG (B) | V2 | | | | | | |
| 6 | | MOVING_AVG (B) | V3 | | | | | | |
| 7 | | KALMAN (C) | V1 | | | | | | Gen Alpha Q/R tuned |
| 8 | | KALMAN (C) | V2 | | | | | | |
| 9 | | KALMAN (C) | V3 | | | | | | |
| 10 | | HYBRID (D) | V1 | | | | | | Spike gate active |
| 11 | | HYBRID (D) | V2 | | | | | | |
| 12 | | HYBRID (D) | V3 | | | | | | |

---

### Observations

*To be completed as experiments are run. Record: what changed, what was observed, any anomalies.*

**Run 1–3 (Baseline):**  
>

**Run 4–6 (Moving Average):**  
>

**Run 7–9 (Kalman):**  
>

**Run 10–12 (Hybrid):**  
>

---

### Analysis

*Which filter iteration performed best against the success criteria?*
*Were there any unexpected technical findings?*

>

---

### Conclusion

*State which filter and PSM variant is selected for Phase 2 scale testing, and why.*

>

---

## Experiment 1.2 — Pairing State Machine Architecture

**Research question:** What is the optimal firmware state machine architecture
for reliable, low-latency device pairing using the 3D magnetic sensor?

**Technical unknowns being resolved:**
1. Whether a dwell confirmation window reduces false pairings enough to justify the added latency
2. Whether Z-axis directional locking provides meaningful false-positive reduction in practice
3. Optimal timeout and reconnect window values for Gen Alpha use patterns

---

### Firmware Change Log

*Record each firmware iteration as it is developed — required for ATO substantiation.*

| Date | Commit | Change Description | Rationale |
|------|--------|--------------------|-----------|
| | | | |

---

### Phase 2 Test Plan Reference

Following completion of Experiment 1.1 and 1.2, the best-performing
filter + PSM variant combination progresses to Phase 2:
**Controlled Lab Testing (Weeks 7–12)**.

Refer to: `Zemi_RD_2026_Scope_Hypothesis_TestPlans.docx`, Section 3.3.

---

*All experiment records maintained in accordance with ATO R&D Tax Incentive substantiation requirements.*  
*Zemi Pty Ltd — Confidential — April 2026*
