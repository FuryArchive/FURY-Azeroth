#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
export MYSQL_PORT="${MYSQL_PORT:-3306}"
export MYSQL_USER="${MYSQL_USER:-root}"
export MYSQL_PASSWORD="${MYSQL_PASSWORD:-fury}"
export MYSQL_PWD="${MYSQL_PASSWORD}"

run_case() {
  local id="$1"
  local title="$2"
  shift 2
  echo
  echo "============================================================"
  echo "[FURY][T34] ${id} — ${title}"
  echo "============================================================"
  "$@"
  echo "[FURY][PASS] ${id} — ${title}"
}

# GS10-GS12 live in the graph policy/replay golden:
# - underlevel/invalid actor never starts;
# - one eligible Human starts exactly one run;
# - duplicate/replayed triggers remain one active run.
run_case GS10 "underlevel/no eligibility"   bash "${ROOT}/scripts/test-t25-defias-graph.sh"

run_case GS11 "one valid Director start"   bash "${ROOT}/scripts/test-t25-defias-graph.sh"

run_case GS12 "duplicate simultaneous trigger -> one run"   bash "${ROOT}/scripts/test-t25-defias-schema.sh"

# GS13-GS15 are the durable T32 outcome projections.
run_case GS13 "Success persistence"   env MYSQL_DATABASE=acore_fury   bash "${ROOT}/scripts/test-t32-defias-resolution.sh"

run_case GS14 "Partial persistence and campaign continuation"   env MYSQL_DATABASE=acore_fury   bash "${ROOT}/scripts/test-t32-defias-resolution.sh"

run_case GS15 "Ignored persistence and pressure flag"   env MYSQL_DATABASE=acore_fury   bash "${ROOT}/scripts/test-t32-defias-resolution.sh"

# M1 event-store schema gate contains the durable append/checkpoint/replay
# interruption scenario used by all later consumers.
run_case GS16 "event append + consumer crash replay"   env MYSQL_DATABASE=acore_fury   bash "${ROOT}/scripts/test-m1-schema.sh"

# T33 is the concrete restart/reconciliation executor.
run_case GS17 "restart during Living World run"   env MYSQL_DATABASE=acore_fury   bash "${ROOT}/scripts/test-t33-director-lw-recovery.sh"

# T24 validates missing/unavailable/mismatched Living World authored content
# and the narrow FURY-owned world-DB overlay.
run_case GS18 "missing dependency/definition is safe"   env MYSQL_DATABASE=fury_t34_world   bash "${ROOT}/scripts/test-t24-defias-content.sh"

echo
echo "============================================================"
echo "[FURY][T34] Vertical-slice dependency sweep"
echo "============================================================"

# These are not extra GS numbers; they prove that the gameplay surfaces used
# by the real two-human run remain green in the same acceptance job.
run_case DEP26 "Westfall contract board + world overlay" \
  env MYSQL_DATABASE=fury_t34_board \
  bash "${ROOT}/scripts/test-t26-contract-board.sh"

run_case DEP27 "six Defias contracts + replay"   env MYSQL_DATABASE=acore_fury   bash "${ROOT}/scripts/test-t27-defias-contracts.sh"

run_case DEP28 "human participation authority"   bash "${ROOT}/scripts/test-t28-participation.sh"

run_case DEP29 "exact-run participation scoring"   env MYSQL_DATABASE=acore_fury   bash "${ROOT}/scripts/test-t29-defias-score.sh"

run_case DEP30 "Defias Bestiary + commander mastery"   env MYSQL_DATABASE=acore_fury   bash "${ROOT}/scripts/test-t30-defias-bestiary.sh"

run_case DEP31 "adaptive Field Relief Profession Order"   env MYSQL_DATABASE=acore_fury   bash "${ROOT}/scripts/test-t31-field-relief.sh"

echo
echo "[FURY][PASS] T34 automated Defias vertical-slice suite passed"
