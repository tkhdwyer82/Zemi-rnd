#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TODAY=$(date +%Y-%m-%d)

if [ "$TODAY" = "2026-05-04" ]; then
  echo "Running Day 4 — 04 May 2026"
  bash "$SCRIPT_DIR/day4_commit.sh"
  echo "Slack messages: $SCRIPT_DIR/day4_slack.txt"
  exit 0
fi

if [ "$TODAY" = "2026-05-05" ]; then
  echo "Running Day 5 — 05 May 2026"
  bash "$SCRIPT_DIR/day5_commit.sh"
  echo "Slack messages: $SCRIPT_DIR/day5_slack.txt"
  exit 0
fi

if [ "$TODAY" = "2026-05-06" ]; then
  echo "Running Day 6 — 06 May 2026"
  bash "$SCRIPT_DIR/day6_commit.sh"
  echo "Slack messages: $SCRIPT_DIR/day6_slack.txt"
  exit 0
fi

if [ "$TODAY" = "2026-05-07" ]; then
  echo "Running Day 7 — 07 May 2026"
  bash "$SCRIPT_DIR/day7_commit.sh"
  echo "Slack messages: $SCRIPT_DIR/day7_slack.txt"
  exit 0
fi

echo "No automation scheduled for today ($TODAY)"
