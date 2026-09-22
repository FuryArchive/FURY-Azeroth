#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

run_gate() {
  local label="$1"
  shift
  echo
  echo "============================================================"
  echo "[FURY][T20] ${label}"
  echo "============================================================"
  "$@"
}

echo "[FURY][T20] M2 Campaign Platform automated acceptance suite"
echo "[FURY][T20] Running M1 regression + T13-T19 mandatory golden scenarios"

# M1 regression remains mandatory for M2 acceptance.
run_gate "M1 actor-policy regression" bash "${ROOT}/scripts/test-m1-actor-policy.sh"
run_gate "M1 schema/replay/reward/Chronicle regression" bash "${ROOT}/scripts/test-m1-schema.sh"

# Fast policy/service-boundary tests.
run_gate "T13 Campaign transition policy" bash "${ROOT}/scripts/test-m2-campaign-policy.sh"
run_gate "T14 Proof authority/idempotency policy" bash "${ROOT}/scripts/test-m2-proof-policy.sh"
run_gate "T15 Contract authority/index policy" bash "${ROOT}/scripts/test-m2-contract-policy.sh"
run_gate "T16 Director authority/reconciliation policy" bash "${ROOT}/scripts/test-m2-director-policy.sh"
run_gate "T17 Profession Order authority/target filtering policy" bash "${ROOT}/scripts/test-m2-profession-policy.sh"
run_gate "T18 Bestiary authority/level filtering policy" bash "${ROOT}/scripts/test-m2-bestiary-policy.sh"
run_gate "T19 Individual Progression read-only/absence/access policy" bash "${ROOT}/scripts/test-m2-ip-adapter.sh"

# Persistent/schema scenarios. Each gate recreates its own canonical fixture via
# test-m1-schema.sh, so scenarios are isolated and deterministic.
run_gate "T13 Campaign persistence + transition schema scenarios" bash "${ROOT}/scripts/test-m2-campaign-schema.sh"
run_gate "T14 Proof persistence/idempotency schema scenarios" bash "${ROOT}/scripts/test-m2-proof-schema.sh"
run_gate "T15 Contract replay/crash-recovery schema scenarios" bash "${ROOT}/scripts/test-m2-contracts-schema.sh"
run_gate "T16 Director duplicate-start + restart schema scenarios" bash "${ROOT}/scripts/test-m2-director-schema.sh"
run_gate "T17 Profession event filtering/replay schema scenarios" bash "${ROOT}/scripts/test-m2-professions-schema.sh"
run_gate "T18 Bestiary event filtering/replay schema scenarios" bash "${ROOT}/scripts/test-m2-bestiary-schema.sh"
run_gate "T19 household/character gate separation schema scenarios" bash "${ROOT}/scripts/test-m2-ip-schema.sh"

echo
echo "[FURY][PASS] T20 M2 Campaign Platform automated gate passed"
