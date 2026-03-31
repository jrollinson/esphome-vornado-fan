#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PASS=0
FAIL=0

for config in "$SCRIPT_DIR"/*.yaml; do
  name="$(basename "$config")"
  echo "--- $name ---"
  if esphome compile "$config"; then
    PASS=$((PASS + 1))
  else
    FAIL=$((FAIL + 1))
  fi
  echo
done

echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
