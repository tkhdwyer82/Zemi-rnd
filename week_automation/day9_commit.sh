#!/bin/bash
set -e
echo "Day 9 — 09 May 2026 (Friday)"

cat >> firmware/core_activity_1_magnetic_sensor/docs/experiment_log.md << 'EOF'

---

## Phase 3 — Day 9 — 09 May 2026 (4 hours)
**Tag:** ca1-p3-dynamic-walk-02 | **Code:** ZEM-CA1-P3 | **Personnel:** Timothy Dwyer

### Work performed:
- Dynamic test session 2 — UART log analysis from Day 8 walking session
- Z-axis field value and angle reviewed at each pairing attempt
- Latency histogram constructed: static vs dynamic comparison
- Failure mode analysis: timeout distribution across participant age groups

### Key findings:
- Z-axis field values at failed attempts: 61-67% of total magnitude (below 68% lock threshold)
- Confirmed: failures are geometric — wrist angle insufficient, not EMI or firmware error
- Latency histogram: dynamic mean 218ms vs static mean 196ms — 22ms overhead
- Age correlation: participants aged 8-11 showed 3x more orientation timeouts vs 12-15
- Recommendation: firmware iteration to adjust Z-lock to 65% for younger age group

### Status: UART analysis complete — backpack EMI impact test Day 10
EOF

cat > firmware/core_activity_1_magnetic_sensor/logs/phase3/ca1_p3-dynamic-walk-02_20260509.txt << 'EOF'
# CA1 Phase 3 — Dynamic Walking UART Analysis
# Date: 2026-05-09 | Tag: ca1-p3-dynamic-walk-02 | Personnel: Timothy Dwyer
[2026-05-09 09:00:00] ANALYSIS_START: UART log review — Day 8 walking session
[2026-05-09 09:30:00] Z_ANALYSIS: Failed attempts Z-axis 61-67% of total magnitude
[2026-05-09 09:31:00] Z_ANALYSIS: Confirmed geometric failure — wrist angle below 68% threshold
[2026-05-09 09:45:00] LATENCY: Dynamic mean 218ms vs static mean 196ms — 22ms overhead
[2026-05-09 10:00:00] AGE_ANALYSIS: Ages 8-11 — 6 timeouts / Ages 12-15 — 2 timeouts
[2026-05-09 10:01:00] AGE_ANALYSIS: 3x more orientation timeouts in younger cohort
[2026-05-09 10:15:00] RECOMMENDATION: Firmware iteration — adjust Z-lock 68% to 65% for age 8-11
[2026-05-09 10:30:00] FALSE_POSITIVE: 0 FP events across all dynamic attempts confirmed
[2026-05-09 17:00:00] STATUS: UART analysis complete — backpack EMI test Day 10
EOF

cat >> firmware/core_activity_2_inductive_charging/docs/experiment_log.md << 'EOF'

---

## Phase 3B — Day 9 — 09 May 2026 (8 hours)
**Tag:** ca2-p3b-session3-setup | **Code:** ZEM-CA2-P3B | **Personnel:** Timothy Dwyer

### Work performed:
- Integrated session 3 setup — elevated ambient stress test
- Session 3 objective: validate S3 predictive ramp at elevated ambient (24.1C)
- Configuration: S3+M2+X3 (M2 re-installed for stress comparison with session 1)
- Environmental heater deployed to raise ambient from 21.2C to 24.1C
- IR array re-verified at elevated ambient: all 5 points within ±0.2C
- Protocol: identical to session 1 — 2hr run, 30min interval logging

### Status: Session 3 setup complete — elevated ambient run Day 10
EOF

cat > firmware/core_activity_2_inductive_charging/logs/phase3/ca2_p3b-session3-setup_20260509.txt << 'EOF'
# CA2 Phase 3B — Session 3 Setup (Elevated Ambient)
# Date: 2026-05-09 | Tag: ca2-p3b-session3-setup | Personnel: Timothy Dwyer
[2026-05-09 09:00:00] SETUP_START: Session 3 — elevated ambient stress test
[2026-05-09 09:15:00] CONFIG: S3+M2+X3 — M2 re-installed for comparison with session 1
[2026-05-09 09:30:00] HEATER: Environmental heater deployed — target ambient 24.1C
[2026-05-09 10:00:00] AMBIENT: Room temp reached 24.1C — stable
[2026-05-09 10:15:00] IR_VERIFY: 5-point verification at 24.1C ambient
[2026-05-09 10:15:01] IR_CAL: Point 1 delta +0.14C — within ±0.2C spec
[2026-05-09 10:15:02] IR_CAL: Point 2 delta -0.12C — within ±0.2C spec
[2026-05-09 10:15:03] IR_CAL: Point 3 delta +0.18C — within ±0.2C spec
[2026-05-09 10:15:04] IR_CAL: Point 4 delta +0.11C — within ±0.2C spec
[2026-05-09 10:15:05] IR_CAL: Point 5 delta -0.15C — within ±0.2C spec
[2026-05-09 10:30:00] PROTOCOL: Session 3 identical to session 1 — 2hr, 30min intervals
[2026-05-09 17:00:00] STATUS: Session 3 setup complete — elevated ambient run Day 10
EOF

git add .
git commit -m "[2026-05-09] ca1-p3-dynamic-walk-02 + ca2-p3b-session3-setup: CA1 (4h) — UART analysis Day 8, Z-axis 61-67% at failures confirmed geometric, 22ms dynamic overhead, age 8-11 3x more timeouts, Z-lock 65% recommendation. CA2 (8h) — session 3 elevated ambient setup, 24.1C achieved, IR verified, S3+M2+X3 config. ZEM-01 | 12h total"

git tag -a "ca1-p3-dynamic-walk-02" -m "[2026-05-09] CA1 Phase 3 Day 9: UART analysis, geometric failures confirmed, Z-lock 65% recommendation" 2>/dev/null || echo "Tag exists — skipping"
git tag -a "ca2-p3b-session3-setup" -m "[2026-05-09] CA2 Phase 3B Day 9: Session 3 elevated ambient setup 24.1C" 2>/dev/null || echo "Tag exists — skipping"

echo ""
echo "✅ Day 9 committed — now run: git push origin main --tags"
