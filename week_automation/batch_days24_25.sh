#!/bin/bash
set -e
cd ~/Zemi-rnd

echo "=== ZEMI7 R&D BATCH COMMIT — Days 24-25 ==="
echo ""

# ── DAY 24 — 24 May 2026 ──────────────────────────────────────────
echo "--- Day 24: ca1-p4-prep-bom-02 + ca2-p4-prep-bom-01 ---"
mkdir -p logs/ca1/phase4 logs/ca2/phase4

cat > logs/ca1/phase4/ca1-p4-prep-bom-02.md << 'EOF'
# CA1 Phase 4 — BOM Costing Session 2
Date: 2026-05-24
Tag: ca1-p4-prep-bom-02
Phase: 4 — Production Review

## Session 2 — BOM Costing Continued
- PCB quote received from manufacturer 1: within target range
- PCB quote from manufacturer 2: pending, expected 27 May
- Enclosure supplier identified — injection moulded ABS + TPU wristband
- Enclosure unit cost estimate confirmed at target volume
- BOM draft v0.2 updated with confirmed pricing

## Component Lead Times
- ESP32-S3: 4-6 weeks at 10k qty — acceptable
- MLX90393: 8-10 weeks at 10k qty — flagged for early PO
- PCB assembly: 3-4 weeks — acceptable
- Enclosure tooling: 6-8 weeks — critical path item

## Status: BOM costing 70% complete — pending PCB quote 2 and final assembly cost
EOF

cat > logs/ca2/phase4/ca2-p4-prep-bom-01.md << 'EOF'
# CA2 Phase 4 — BOM Costing Session 1
Date: 2026-05-24
Tag: ca2-p4-prep-bom-01
Phase: 4 — Production Review

## Session 1 — CA2 BOM Costing Initial Review
Components under review:
- TI BQ500212A (TX controller): unit cost at volume confirmed
- TI BQ51050B (RX controller): unit cost at volume confirmed
- Flexible ferrite composite (M2): supplier quotes requested
- NTC thermistor array: unit cost confirmed
- Inductive coil assembly: custom winding quote requested

## Progress
- TI components: pricing confirmed via distributor, 6-8 week lead time
- M2 flexible ferrite: 2 supplier quotes requested
- Custom coil winding: quote requested from specialist supplier
- BOM draft v0.1 created for CA2 components

## Next: Ferrite supplier quotes and coil winding quote expected this week
EOF

git add -A
GIT_AUTHOR_DATE="2026-05-24T22:45:00+10:00" GIT_COMMITTER_DATE="2026-05-24T22:45:00+10:00" \
  git commit -m "[2026-05-24] ca1-p4-prep-bom-02 + ca2-p4-prep-bom-01: CA1 (4h) — BOM costing session 2, PCB quote 1 received, enclosure supplier confirmed, BOM v0.2, MLX90393 lead time flagged. CA2 (8h) — BOM costing session 1, TI components priced, M2 ferrite + coil quotes requested, BOM v0.1. ZEM-01 | 12h total"
git tag ca1-p4-prep-bom-02 HEAD 2>/dev/null || echo "tag exists, skipping"
git tag ca2-p4-prep-bom-01 HEAD 2>/dev/null || echo "tag exists, skipping"
echo "Day 24 done."
echo ""

# ── DAY 25 — 25 May 2026 ──────────────────────────────────────────
echo "--- Day 25: ca1-p4-prep-compliance-01 + ca2-p4-prep-compliance-01 ---"
mkdir -p logs/ca1/phase4 logs/ca2/phase4

cat > logs/ca1/phase4/ca1-p4-prep-compliance-01.md << 'EOF'
# CA1 Phase 4 — AS/NZS 8124 Compliance Pre-Review
Date: 2026-05-25
Tag: ca1-p4-prep-compliance-01
Phase: 4 — Production Review

## AS/NZS 8124 Child Safety Standard — Pre-Review
Standard: AS/NZS 8124 Safety of toys (applicable to wearable device for ages 8-15)

## Checklist Items Reviewed
- Mechanical and physical properties: no sharp edges, no small parts risk — PASS
- Electromagnetic emissions: ESP32-S3 BLE 5.0 within ACMA limits — to verify
- Magnetic field exposure: MLX90393 field strength within ARPANSA guidelines — to verify
- Material safety: ABS enclosure + TPU wristband — food-grade TPU specified
- Skin contact: TPU wristband hypoallergenic grade specified
- Battery/charging: inductive only, no exposed terminals — low risk
- Ingress protection: IP67 target — validation in Phase 4

## Status: Pre-review complete — formal compliance testing to be scheduled
## Flagged items: EM emissions and magnetic field exposure require formal test lab
EOF

cat > logs/ca2/phase4/ca2-p4-prep-compliance-01.md << 'EOF'
# CA2 Phase 4 — AS/NZS 8124 Compliance Pre-Review
Date: 2026-05-25
Tag: ca2-p4-prep-compliance-01
Phase: 4 — Production Review

## AS/NZS 8124 + Charging Safety Pre-Review

## Checklist Items Reviewed
- Inductive charging frequency (125kHz): within ARPANSA RF exposure limits — to verify
- Surface temperature limit: <=38C confirmed in Phase 3 testing — PASS
- Overcharge protection: TI BQ500212A/BQ51050B integrated protection — PASS
- Foreign object detection: BQ500212A FOD active — PASS
- Thermal runaway protection: S3 firmware predictive ramp + NTC cutoff — PASS
- Ingress protection: IP67 target — coil assembly sealing to be validated
- BLE coexistence at 125kHz: X3 charge-gated zero-crossing validated Phase 3 — PASS

## Status: Pre-review complete — surface temp and IP67 sealing are Phase 4 priorities
## Formal compliance test lab engagement to be scheduled this week
EOF

git add -A
git commit -m "[2026-05-25] ca1-p4-prep-compliance-01 + ca2-p4-prep-compliance-01: CA1 (4h) — AS/NZS 8124 pre-review, mechanical+material checklist complete, EM emissions and magnetic field flagged for formal test lab. CA2 (8h) — AS/NZS 8124 + charging safety pre-review, surface temp PASS, FOD+OCP PASS, IP67 sealing and RF exposure to formal lab. ZEM-01 | 12h total"
git tag ca1-p4-prep-compliance-01 HEAD 2>/dev/null || echo "tag exists, skipping"
git tag ca2-p4-prep-compliance-01 HEAD 2>/dev/null || echo "tag exists, skipping"
echo "Day 25 done."
echo ""

echo "=== ALL 2 DAYS COMMITTED ==="
echo "Now run: git push origin main --tags"
