#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-}"
HOUSEHOLD_SLUG="${2:-}"
SNAPSHOT="${3:-}"

if [[ "${MODE}" != "snapshot" && "${MODE}" != "post-restart" ]]; then
  echo "Usage: $0 <snapshot|post-restart> <household-slug> [snapshot-file]" >&2
  exit 2
fi

if [[ -z "${HOUSEHOLD_SLUG}" ]]; then
  echo "[FURY][FAIL] household slug is required" >&2
  exit 2
fi

if [[ -z "${SNAPSHOT}" ]]; then
  SNAPSHOT="t34-${HOUSEHOLD_SLUG}.snapshot"
fi

export MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
export MYSQL_PORT="${MYSQL_PORT:-3306}"
export MYSQL_USER="${MYSQL_USER:-root}"
export MYSQL_PASSWORD="${MYSQL_PASSWORD:-fury}"
export MYSQL_DATABASE="${MYSQL_DATABASE:-acore_fury}"
export MYSQL_PWD="${MYSQL_PASSWORD}"

mysql_cmd=(
  mysql --protocol=tcp
  --host="${MYSQL_HOST}"
  --port="${MYSQL_PORT}"
  --user="${MYSQL_USER}"
  --batch --skip-column-names
  "${MYSQL_DATABASE}"
)

sql() { "${mysql_cmd[@]}" -e "$1"; }

fail() {
  echo "[FURY][FAIL] $*" >&2
  exit 1
}

pass() {
  echo "[FURY][PASS] $*"
}

household_id="$(sql "SELECT id FROM fury_household WHERE slug='${HOUSEHOLD_SLUG}' LIMIT 1;")"
[[ -n "${household_id}" ]] || fail "household '${HOUSEHOLD_SLUG}' not found"

members="$(sql "SELECT COUNT(*) FROM fury_household_member WHERE household_id=${household_id};")"
[[ "${members}" == "2" ]] || fail "expected exactly two Human accounts in household, got ${members}"
pass "household has exactly two accounts"

run_row="$(sql "SELECT CONCAT(
  id,'|',status,'|',phase_key,'|',
  COALESCE(external_runtime_id,0),'|',
  COALESCE(resolved_event_id,0),'|',
  COALESCE(outcome_key,''),'|',revision)
  FROM fury_director_run
  WHERE household_id=${household_id}
    AND graph_key='classic.westfall.defias_resurgence.v1'
  ORDER BY id DESC LIMIT 1;")"
[[ -n "${run_row}" ]] || fail "no Defias Director run found"

IFS='|' read -r run_id run_status run_phase runtime_id resolved_event_id outcome revision <<<"${run_row}"

[[ "${run_status}" == "4" ]] || fail "latest Defias run is not Complete (status=${run_status})"
case "${outcome}" in
  success|partial|ignored) ;;
  *) fail "unexpected Defias outcome '${outcome}'" ;;
esac
[[ "${resolved_event_id}" != "0" ]] || fail "Director run has no canonical resolved_event_id"
pass "Director run ${run_id} resolved as ${outcome}"

active_runs="$(sql "SELECT COUNT(*) FROM fury_director_run
  WHERE household_id=${household_id}
    AND scope_key='classic.westfall'
    AND status IN (1,2,3);")"
[[ "${active_runs}" == "0" ]] || fail "expected no active Westfall Director run during acceptance snapshot"
pass "no duplicate active Director run"

director_terminal_count="$(sql "SELECT COUNT(*) FROM fury_event
  WHERE id=${resolved_event_id}
    AND event_type='director.run.resolved'
    AND subject_type='director_run'
    AND subject_id=${run_id}
    AND source_system='fury.director';")"
[[ "${director_terminal_count}" == "1" ]] || fail "canonical Director terminal event is missing or duplicated"

resolution_count="$(sql "SELECT COUNT(*) FROM fury_event
  WHERE event_type='defias.resolution.${outcome}'
    AND subject_type='director_run'
    AND subject_id=${run_id}
    AND source_system='fury.defias';")"
[[ "${resolution_count}" == "1" ]] || fail "expected exactly one defias.resolution.${outcome} event"

completed_contracts="$(sql "SELECT COUNT(*) FROM fury_contract_instance
  WHERE household_id=${household_id}
    AND director_run_id=${run_id}
    AND status=3;")"
(( completed_contracts >= 1 )) || fail "no completed Defias contract for run ${run_id}"
pass "at least one Defias contract completed"

completed_orders="$(sql "SELECT COUNT(*) FROM fury_profession_order_instance
  WHERE household_id=${household_id}
    AND order_key='classic.westfall.defias.field_relief'
    AND status=3;")"
(( completed_orders >= 1 )) || fail "Field Relief Profession Order did not complete"
pass "Field Relief Profession Order completed"

bestiary_rows="$(sql "SELECT COUNT(*)
  FROM fury_bestiary_state s
  JOIN fury_household_member h ON h.account_id=s.account_id
  WHERE h.household_id=${household_id}
    AND s.entry_key LIKE 'classic.westfall.defias.%'
    AND s.discovery_level >= 2;")"
(( bestiary_rows >= 1 )) || fail "no household account reached Studied on Defias Bestiary content"
pass "Defias Bestiary progressed"

bestiary_checksum="$(sql "SELECT COALESCE(GROUP_CONCAT(
  CONCAT(s.account_id,':',s.entry_key,':',s.discovery_level,':',s.kill_count,':',s.revision)
  ORDER BY s.account_id,s.entry_key SEPARATOR ','),'')
  FROM fury_bestiary_state s
  JOIN fury_household_member h ON h.account_id=s.account_id
  WHERE h.household_id=${household_id}
    AND s.entry_key LIKE 'classic.westfall.defias.%';")"

chronicle_key="classic.westfall.defias.resolution.${outcome}"
chronicle_count="$(sql "SELECT COUNT(*) FROM fury_chronicle_entry
  WHERE household_id=${household_id}
    AND entry_key='${chronicle_key}';")"
[[ "${chronicle_count}" == "1" ]] || fail "expected exactly one outcome Chronicle entry"
pass "Chronicle outcome persisted once"

case "${outcome}" in
  success)
    proof_key="world.westfall.defended"
    expected_campaign_status=4
    ;;
  partial)
    proof_key="world.westfall.bloodied"
    expected_campaign_status=3
    ;;
  ignored)
    proof_key="world.westfall.emergency"
    expected_campaign_status=3
    ;;
esac

proof_count="$(sql "SELECT COUNT(*) FROM fury_proof
  WHERE household_id=${household_id}
    AND proof_key='${proof_key}';")"
[[ "${proof_count}" == "1" ]] || fail "expected exactly one ${proof_key} proof"

campaign_status="$(sql "SELECT status FROM fury_campaign_state
  WHERE household_id=${household_id}
    AND node_key='campaign.classic.westfall';")"
[[ "${campaign_status}" == "${expected_campaign_status}" ]] ||
  fail "campaign status mismatch for ${outcome}: expected ${expected_campaign_status}, got ${campaign_status}"
pass "campaign/proof state matches ${outcome}"

reward_claims="$(sql "SELECT COUNT(*) FROM fury_reward_claim
  WHERE reward_key='classic.westfall.defias.success'
    AND beneficiary_kind=3
    AND beneficiary_id=${household_id};")"

if [[ "${outcome}" == "success" ]]; then
  [[ "${reward_claims}" == "1" ]] || fail "Success must create exactly one household reward claim"
else
  [[ "${reward_claims}" == "0" ]] || fail "${outcome} must not create the Success reward claim"
fi

if [[ "${outcome}" == "ignored" ]]; then
  pressure_count="$(sql "SELECT COUNT(*) FROM fury_event
    WHERE event_type='defias.pressure.increased'
      AND subject_type='director_run'
      AND subject_id=${run_id};")"
  [[ "${pressure_count}" == "1" ]] || fail "Ignored must record exactly one pressure increase"
else
  pressure_count=0
fi

contracts_checksum="$(sql "SELECT COALESCE(GROUP_CONCAT(
  CONCAT(id,':',status,':',COALESCE(completed_event_id,0),':',revision)
  ORDER BY id SEPARATOR ','),'')
  FROM fury_contract_instance
  WHERE household_id=${household_id}
    AND director_run_id=${run_id};")"

orders_checksum="$(sql "SELECT COALESCE(GROUP_CONCAT(
  CONCAT(id,':',status,':',progress_count,':',COALESCE(completed_event_id,0),':',revision)
  ORDER BY id SEPARATOR ','),'')
  FROM fury_profession_order_instance
  WHERE household_id=${household_id}
    AND order_key='classic.westfall.defias.field_relief';")"

state_payload="$(cat <<EOF
HOUSEHOLD_ID=${household_id}
RUN_ID=${run_id}
OUTCOME=${outcome}
RESOLVED_EVENT_ID=${resolved_event_id}
REVISION=${revision}
COMPLETED_CONTRACTS=${completed_contracts}
COMPLETED_ORDERS=${completed_orders}
BESTIARY_CHECKSUM=${bestiary_checksum}
CHRONICLE_COUNT=${chronicle_count}
PROOF_COUNT=${proof_count}
REWARD_CLAIMS=${reward_claims}
PRESSURE_COUNT=${pressure_count}
CONTRACTS_CHECKSUM=${contracts_checksum}
ORDERS_CHECKSUM=${orders_checksum}
EOF
)"

if [[ "${MODE}" == "snapshot" ]]; then
  printf '%s
' "${state_payload}" > "${SNAPSHOT}"
  pass "T34 pre-restart snapshot written to ${SNAPSHOT}"
  echo "[FURY] Stop and restart worldserver now, without additional gameplay."
  echo "[FURY] Then run: $0 post-restart ${HOUSEHOLD_SLUG} ${SNAPSHOT}"
  exit 0
fi

[[ -f "${SNAPSHOT}" ]] || fail "snapshot file '${SNAPSHOT}' does not exist"

# shellcheck disable=SC1090
source "${SNAPSHOT}"

[[ "${HOUSEHOLD_ID}" == "${household_id}" ]] || fail "household id changed across restart"
[[ "${RUN_ID}" == "${run_id}" ]] || fail "latest Director run changed across restart"
[[ "${OUTCOME}" == "${outcome}" ]] || fail "outcome changed across restart"
[[ "${RESOLVED_EVENT_ID}" == "${resolved_event_id}" ]] || fail "resolved event id changed across restart"
[[ "${REVISION}" == "${revision}" ]] || fail "Director revision changed across idle restart"
[[ "${COMPLETED_CONTRACTS}" == "${completed_contracts}" ]] || fail "contract completion count changed across restart"
[[ "${COMPLETED_ORDERS}" == "${completed_orders}" ]] || fail "Profession Order completion count changed across restart"
[[ "${BESTIARY_CHECKSUM}" == "${bestiary_checksum}" ]] || fail "Bestiary state changed across idle restart"
[[ "${CHRONICLE_COUNT}" == "${chronicle_count}" ]] || fail "Chronicle outcome duplicated across restart"
[[ "${PROOF_COUNT}" == "${proof_count}" ]] || fail "proof state changed across restart"
[[ "${REWARD_CLAIMS}" == "${reward_claims}" ]] || fail "reward claim count changed across restart"
[[ "${PRESSURE_COUNT}" == "${pressure_count}" ]] || fail "pressure event count changed across restart"
[[ "${CONTRACTS_CHECKSUM}" == "${contracts_checksum}" ]] || fail "contract state changed across restart"
[[ "${ORDERS_CHECKSUM}" == "${orders_checksum}" ]] || fail "Profession Order state changed across restart"

pass "restart preserved Director, contracts, Profession Order, Bestiary, Chronicle, proof, campaign and reward state without duplication"
echo "[FURY][PASS] T34 live acceptance verifier passed"
