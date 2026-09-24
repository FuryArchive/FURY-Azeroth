#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROFILE="${1:-${ROOT}/build/playable-profile}"
SQL_ROOT="${PROFILE}/server/sql"

MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
MYSQL_PORT="${MYSQL_PORT:-3306}"
MYSQL_USER="${MYSQL_USER:-root}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-}"
WORLD_DB="${FURY_WORLD_DB:-acore_world}"
CHAR_DB="${FURY_CHARACTER_DB:-acore_characters}"

fail() { echo "[FURY][SQL][FAIL] $*" >&2; exit 1; }
[[ -d "${SQL_ROOT}" ]] || fail "playable SQL payload missing: ${SQL_ROOT}"

mysql_args=(--protocol=tcp --host="${MYSQL_HOST}" --port="${MYSQL_PORT}" --user="${MYSQL_USER}")
if [[ -n "${MYSQL_PASSWORD}" ]]; then
  mysql_args+=(--password="${MYSQL_PASSWORD}")
fi

apply_tree() {
  local db="$1"
  local dir="$2"
  [[ -d "${dir}" ]] || return 0

  while IFS= read -r -d '' sql; do
    echo "[FURY][SQL] ${db} <- ${sql#${PROFILE}/}"
    mysql "${mysql_args[@]}" "${db}" < "${sql}"
  done < <(find "${dir}" -maxdepth 1 -type f -name '*.sql' -print0 | sort -z)
}

mysql "${mysql_args[@]}" -e "SELECT 1" >/dev/null || fail "MySQL connection failed"
mysql "${mysql_args[@]}" -e "USE \`${WORLD_DB}\`; SELECT 1" >/dev/null || fail "world DB missing: ${WORLD_DB}"
mysql "${mysql_args[@]}" -e "USE \`${CHAR_DB}\`; SELECT 1" >/dev/null || fail "character DB missing: ${CHAR_DB}"

apply_tree "${WORLD_DB}" "${SQL_ROOT}/world/delves"
apply_tree "${WORLD_DB}" "${SQL_ROOT}/world/mythicplus"
apply_tree "${CHAR_DB}" "${SQL_ROOT}/characters/mythicplus"

mysql "${mysql_args[@]}" "${WORLD_DB}" -Nse "SELECT COUNT(*) FROM creature_template WHERE entry = 900001" | grep -Eq '^[1-9][0-9]*$' \
  || fail "Mythic+ pedestal creature 900001 missing after SQL import"

mysql "${mysql_args[@]}" "${WORLD_DB}" -Nse "SELECT COUNT(*) FROM instance_template WHERE map BETWEEN 900 AND 911 OR map = 805" | grep -Eq '^[1-9][0-9]*$' \
  || fail "Delves instance templates missing after SQL import"

mysql "${mysql_args[@]}" "${CHAR_DB}" -Nse "SHOW TABLES LIKE 'character_mythic_keys'" | grep -Fxq 'character_mythic_keys' \
  || fail "Mythic+ character tables missing after SQL import"

echo "[FURY][SQL][PASS] playable SQL imported and verified"
