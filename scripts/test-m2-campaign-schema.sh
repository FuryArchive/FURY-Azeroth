#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
MYSQL_PORT="${MYSQL_PORT:-3306}"
MYSQL_USER="${MYSQL_USER:-root}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-fury}"
MYSQL_DATABASE="${MYSQL_DATABASE:-acore_fury}"

export MYSQL_HOST MYSQL_PORT MYSQL_USER MYSQL_PASSWORD MYSQL_DATABASE
export MYSQL_PWD="${MYSQL_PASSWORD}"

bash "${ROOT}/scripts/test-m1-schema.sh"

mysql_cmd=(
  mysql --protocol=tcp
  --host="${MYSQL_HOST}"
  --port="${MYSQL_PORT}"
  --user="${MYSQL_USER}"
  --batch --skip-column-names
  "${MYSQL_DATABASE}"
)

sql() { "${mysql_cmd[@]}" -e "$1"; }

assert_eq() {
  local expected="$1"
  local actual="$2"
  local message="$3"
  if [[ "${actual}" != "${expected}" ]]; then
    echo "[FURY][FAIL] ${message}: expected=${expected} actual=${actual}" >&2
    exit 1
  fi
  echo "[FURY][PASS] ${message}"
}

assert_campaign_tables() {
  for table in fury_campaign_node fury_campaign_state; do
    present="$(sql "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='${MYSQL_DATABASE}' AND table_name='${table}';")"
    assert_eq "1" "${present}" "table ${table} exists"
  done
}

echo "[FURY] fresh-install Campaign schema"
assert_campaign_tables

echo "[FURY] M1 -> T13 migration path"
# Downstream schemas may reference Campaign once later M2 tasks exist. Remove
# those consumers before simulating the historical M1 -> T13 upgrade.
sql "DROP TABLE IF EXISTS fury_contract_progress;
     DROP TABLE IF EXISTS fury_contract_instance;
     DROP TABLE IF EXISTS fury_contract_objective;
     DROP TABLE IF EXISTS fury_contract;
     DROP TABLE fury_campaign_state;
     DROP TABLE fury_campaign_node;"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_22_01_campaign.sql"
assert_campaign_tables

household_id="$(sql "SELECT id FROM fury_household WHERE slug='alpha' LIMIT 1;")"
source_event_id="$(sql "SELECT MIN(id) FROM fury_event;")"

sql "INSERT INTO fury_campaign_node
  (node_key, era, ordinal, display_name, required_power_band, grants_power_band, enabled)
  VALUES
  ('golden.campaign', 1, 1, 'Golden Campaign', 0, 110, 1),
  ('golden.campaign.high', 1, 2, 'Golden Campaign High', 110, 120, 1);"

echo "[FURY] optimistic campaign transitions"
sql "INSERT INTO fury_campaign_state
  (household_id, node_key, status, source_event_id, revision)
  VALUES (${household_id}, 'golden.campaign', 2, ${source_event_id}, 0);"

sql "UPDATE fury_campaign_state
  SET status=3,
      activated_at=CASE WHEN activated_at IS NULL THEN CURRENT_TIMESTAMP(6) ELSE activated_at END,
      source_event_id=${source_event_id},
      revision=revision+1
  WHERE household_id=${household_id}
    AND node_key='golden.campaign'
    AND revision=0;"

assert_eq "3" "$(sql "SELECT status FROM fury_campaign_state WHERE household_id=${household_id} AND node_key='golden.campaign';")" "forward transition applies"
assert_eq "1" "$(sql "SELECT revision FROM fury_campaign_state WHERE household_id=${household_id} AND node_key='golden.campaign';")" "campaign revision advances"
assert_eq "1" "$(sql "SELECT activated_at IS NOT NULL FROM fury_campaign_state WHERE household_id=${household_id} AND node_key='golden.campaign';")" "activation timestamp recorded"

sql "UPDATE fury_campaign_state
  SET status=4, revision=revision+1
  WHERE household_id=${household_id}
    AND node_key='golden.campaign'
    AND revision=0;"
assert_eq "3" "$(sql "SELECT status FROM fury_campaign_state WHERE household_id=${household_id} AND node_key='golden.campaign';")" "stale revision cannot overwrite state"

sql "UPDATE fury_campaign_state
  SET status=4,
      completed_at=CASE WHEN completed_at IS NULL THEN CURRENT_TIMESTAMP(6) ELSE completed_at END,
      source_event_id=${source_event_id},
      revision=revision+1
  WHERE household_id=${household_id}
    AND node_key='golden.campaign'
    AND revision=1;"

echo "[FURY] household power band derives only from completed nodes"
sql "INSERT INTO fury_campaign_state
  (household_id, node_key, status, source_event_id, revision)
  VALUES (${household_id}, 'golden.campaign.high', 3, ${source_event_id}, 0);"

derived="$(sql "SELECT COALESCE(MAX(n.grants_power_band),0)
  FROM fury_campaign_state s
  JOIN fury_campaign_node n ON n.node_key=s.node_key
  WHERE s.household_id=${household_id} AND s.status=4;")"
assert_eq "110" "${derived}" "active higher node does not raise power band"

sql "UPDATE fury_household
  SET current_power_band=${derived}, revision=revision+1
  WHERE id=${household_id} AND current_power_band<>${derived};"
assert_eq "110" "$(sql "SELECT current_power_band FROM fury_household WHERE id=${household_id};")" "completed node advances household power band"

revision_before="$(sql "SELECT revision FROM fury_household WHERE id=${household_id};")"
sql "UPDATE fury_household
  SET current_power_band=${derived}, revision=revision+1
  WHERE id=${household_id} AND current_power_band<>${derived};"
assert_eq "${revision_before}" "$(sql "SELECT revision FROM fury_household WHERE id=${household_id};")" "idempotent repair does not churn household revision"

sql "UPDATE fury_campaign_state
  SET status=4,
      completed_at=CASE WHEN completed_at IS NULL THEN CURRENT_TIMESTAMP(6) ELSE completed_at END,
      revision=revision+1
  WHERE household_id=${household_id}
    AND node_key='golden.campaign.high'
    AND revision=0;"

derived="$(sql "SELECT COALESCE(MAX(n.grants_power_band),0)
  FROM fury_campaign_state s
  JOIN fury_campaign_node n ON n.node_key=s.node_key
  WHERE s.household_id=${household_id} AND s.status=4;")"
assert_eq "120" "${derived}" "completed higher node raises power band"

# Atomic repository-equivalent recalculation: the SQL derives the current
# canonical value inside the UPDATE, so a stale caller cannot write 110 after
# the higher node has already completed.
sql "UPDATE fury_household h
  JOIN (
    SELECT ${household_id} AS household_id, COALESCE(MAX(n.grants_power_band),0) AS derived_band
    FROM fury_campaign_state s
    JOIN fury_campaign_node n ON n.node_key=s.node_key
    WHERE s.household_id=${household_id} AND s.status=4
  ) d ON d.household_id=h.id
  SET h.current_power_band=d.derived_band, h.revision=h.revision+1
  WHERE h.current_power_band<>d.derived_band;"
assert_eq "120" "$(sql "SELECT current_power_band FROM fury_household WHERE id=${household_id};")" "atomic cache repair follows latest canonical completion"

echo "[FURY] schema re-apply preserves state"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/base/50_campaign.sql"
"${mysql_cmd[@]}" < "${ROOT}/modules/mod-fury/data/sql/fury/updates/2026_09_22_01_campaign.sql"
assert_eq "4" "$(sql "SELECT status FROM fury_campaign_state WHERE household_id=${household_id} AND node_key='golden.campaign';")" "campaign state survives schema re-apply"
assert_eq "120" "$(sql "SELECT current_power_band FROM fury_household WHERE id=${household_id};")" "power band survives schema re-apply"

echo "[FURY][PASS] T13 Campaign schema golden gate passed"
