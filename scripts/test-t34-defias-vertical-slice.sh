#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
export MYSQL_PORT="${MYSQL_PORT:-3306}"
export MYSQL_USER="${MYSQL_USER:-root}"
export MYSQL_PASSWORD="${MYSQL_PASSWORD:-fury}"
export MYSQL_PWD="${MYSQL_PASSWORD}"

run_db() {
  local db="$1"
  shift
  MYSQL_DATABASE="${db}" "$@"
}

echo "[FURY][T34] GS10 — underlevel / no eligibility"
bash "${ROOT}/scripts/test-t25-defias-graph.sh"

echo "[FURY][T34] GS11 — one valid start"
echo "[FURY][T34] GS12 — duplicate simultaneous trigger -> one run"
run_db fury_t34_start bash "${ROOT}/scripts/test-t25-defias-schema.sh"

echo "[FURY][T34] GS13 — Success"
echo "[FURY][T34] GS14 — Partial"
echo "[FURY][T34] GS15 — Ignored"
run_db fury_t34_resolution bash "${ROOT}/scripts/test-t32-defias-resolution.sh"

echo "[FURY][T34] GS16 — event append + consumer crash replay"
run_db fury_t34_replay bash "${ROOT}/scripts/test-m1-schema.sh"
bash "${ROOT}/scripts/test-t25-director-replay.sh"

echo "[FURY][T34] GS17 — restart during Living World run"
run_db fury_t34_recovery bash "${ROOT}/scripts/test-t33-director-lw-recovery.sh"

echo "[FURY][T34] GS18 — missing dependency / definition"
run_db fury_t34_world bash "${ROOT}/scripts/test-t24-defias-content.sh"

echo "[FURY][PASS] T34 GS10-GS18 automated vertical-slice suite passed"
