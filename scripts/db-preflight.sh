#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="${ROOT}/upstream/azerothcore-wotlk"

MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
MYSQL_PORT="${MYSQL_PORT:-3306}"
MYSQL_USER="${MYSQL_USER:-root}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-}"

mysql_args=(
  --protocol=tcp
  --host="${MYSQL_HOST}"
  --port="${MYSQL_PORT}"
  --user="${MYSQL_USER}"
  --default-character-set=utf8
  --binary-mode=1
  --max_allowed_packet=1073741824
)
if [[ -n "${MYSQL_PASSWORD}" ]]; then
  mysql_args+=(--password="${MYSQL_PASSWORD}")
fi

fail() {
  echo "[FURY][DB-PREFLIGHT][FAIL] $*" >&2
  exit 1
}

pass() {
  echo "[FURY][DB-PREFLIGHT][PASS] $*"
}

sql_quote() {
  printf "%s" "$1" | sed "s/'/''/g"
}

mysql_exec() {
  mysql "${mysql_args[@]}" "$@"
}

import_file() {
  local db="$1"
  local file="$2"
  echo "[FURY][DB-PREFLIGHT] ${db} <- ${file#${ROOT}/}"
  mysql_exec "${db}" < "${file}"
}

record_update() {
  local db="$1"
  local file="$2"
  local state="$3"
  local name hash qname qhash
  name="$(basename "${file}")"
  hash="$(sha1sum "${file}" | awk '{print toupper($1)}')"
  qname="$(sql_quote "${name}")"
  qhash="$(sql_quote "${hash}")"
  mysql_exec "${db}" -e     "REPLACE INTO updates (name, hash, state, speed) VALUES ('${qname}', '${qhash}', '${state}', 0)"
}

already_applied() {
  local db="$1"
  local file="$2"
  local name qname
  name="$(basename "${file}")"
  qname="$(sql_quote "${name}")"
  [[ "$(mysql_exec "${db}" -Nse "SELECT COUNT(*) FROM updates WHERE name='${qname}'")" != "0" ]]
}

import_base() {
  local db="$1"
  local kind="$2"
  local dir="${CORE}/data/sql/base/db_${kind}"
  [[ -d "${dir}" ]] || fail "base directory missing: ${dir}"

  echo "[FURY][DB-PREFLIGHT] importing base snapshot: ${db}"
  while IFS= read -r -d '' file; do
    import_file "${db}" "${file}"
  done < <(find "${dir}" -maxdepth 1 -type f -name '*.sql' -print0 | sort -z)

  mysql_exec "${db}" -Nse "SHOW TABLES LIKE 'updates'" | grep -Fxq updates     || fail "${db}: base snapshot did not create updates table"
  mysql_exec "${db}" -Nse "SHOW TABLES LIKE 'updates_include'" | grep -Fxq updates_include     || fail "${db}: base snapshot did not create updates_include table"
}

collect_module_files() {
  local module_kind="$1"
  local module_root module_sql child name

  [[ -d "${CORE}/modules" ]] || return 0

  while IFS= read -r -d '' module_root; do
    module_sql="${module_root}/data/sql"
    [[ -d "${module_sql}" ]] || continue

    while IFS= read -r -d '' child; do
      name="$(basename "${child}")"
      [[ "${name}" == *"${module_kind}"* ]] || continue
      find -L "${child}" -maxdepth 10 -type f -name '*.sql' -print0
    done < <(find -L "${module_sql}" -mindepth 1 -maxdepth 1 -type d -print0)
  done < <(find -L "${CORE}/modules" -mindepth 1 -maxdepth 1 -type d -print0)
}

sort_by_basename() {
  awk -F/ '{print $NF "\t" $0}' | sort -k1,1 | cut -f2-
}

assert_unique_basenames() {
  local db="$1"
  shift
  local -a files=("$@")
  local dup
  (( ${#files[@]} > 0 )) || return 0
  dup="$(
    printf '%s\n' "${files[@]}" |
      awk -F/ '{print $NF}' |
      sort |
      uniq -d |
      head -n 1
  )"
  [[ -z "${dup}" ]] || fail "${db}: duplicate SQL update filename detected: ${dup}"
}

apply_group() {
  local db="$1"
  local state="$2"
  shift 2
  local -a files=("$@")
  local file

  (( ${#files[@]} > 0 )) || return 0
  assert_unique_basenames "${db}" "${files[@]}"

  mapfile -t files < <(printf '%s\n' "${files[@]}" | sort_by_basename)

  for file in "${files[@]}"; do
    if already_applied "${db}" "${file}"; then
      echo "[FURY][DB-PREFLIGHT] skip already-applied ${db}/$(basename "${file}")"
      continue
    fi
    import_file "${db}" "${file}"
    record_update "${db}" "${file}" "${state}"
  done
}

apply_updates() {
  local db="$1"
  local core_kind="$2"
  local module_kind="$3"

  local -a released=()
  local -a late=()
  local dir file

  dir="${CORE}/data/sql/updates/db_${core_kind}"
  if [[ -d "${dir}" ]]; then
    mapfile -d '' -t released < <(
      find "${dir}" -maxdepth 10 -type f -name '*.sql' -print0
    )
  fi

  for dir in     "${CORE}/data/sql/updates/pending_db_${core_kind}"     "${CORE}/data/sql/custom/db_${core_kind}"
  do
    [[ -d "${dir}" ]] || continue
    while IFS= read -r -d '' file; do
      late+=("${file}")
    done < <(find "${dir}" -maxdepth 10 -type f -name '*.sql' -print0)
  done

  while IFS= read -r -d '' file; do
    late+=("${file}")
  done < <(collect_module_files "${module_kind}")

  # AzerothCore applies RELEASED first, then PENDING/CUSTOM/MODULE.
  apply_group "${db}" RELEASED "${released[@]}"
  apply_group "${db}" MODULE "${late[@]}"
}

read_integration_dir() {
  python3 - "${ROOT}/vendor/lock/fury.lock.yaml" "$1" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as fh:
    lock = json.load(fh)
print(lock["integrations"][sys.argv[2]]["directory"])
PY
}

import_tree_sorted() {
  local db="$1"
  local dir="$2"
  [[ -d "${dir}" ]] || return 0
  while IFS= read -r -d '' file; do
    import_file "${db}" "${file}"
  done < <(find -L "${dir}" -maxdepth 10 -type f -name '*.sql' -print0 | sort -z)
}

[[ -d "${CORE}/data/sql/base" ]] || fail "synced AzerothCore tree missing: ${CORE}"

for _ in {1..60}; do
  if mysql_exec -e "SELECT 1" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done
mysql_exec -e "SELECT 1" >/dev/null 2>&1 || fail "MySQL is unavailable"

mysql_exec -e "SET GLOBAL max_allowed_packet=1073741824" >/dev/null || true
mysql_exec -e "SET GLOBAL innodb_redo_log_capacity=1073741824" >/dev/null || true

for db in acore_auth acore_characters acore_world; do
  mysql_exec -e "DROP DATABASE IF EXISTS \`${db}\`; CREATE DATABASE \`${db}\` CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci"
done

import_base acore_auth auth
import_base acore_characters characters
import_base acore_world world

apply_updates acore_auth auth auth
apply_updates acore_characters characters characters
apply_updates acore_world world world

# Exact regression guard for the old Worgoblin schema.
if grep -RniE 'INSERT[[:space:]]+INTO[[:space:]]+`item_template`[^;]*`StatsCount`'     "${CORE}/modules/mod-worgoblin/data/sql" >/dev/null 2>&1; then
  fail "mod-worgoblin still contains StatsCount in item_template INSERT"
fi

mysql_exec acore_world -Nse "SELECT COUNT(*) FROM player_race_stats WHERE Race IN (9,12)" |
  grep -Fxq 2 || fail "Worgen/Goblin race stats were not imported"

# Playable-only SQL runs after the module updater in the real profile.
DELVES="${ROOT}/upstream/integrations/$(read_integration_dir delves)"
MYTHIC="${ROOT}/upstream/integrations/$(read_integration_dir mythic_plus_extended)"

import_tree_sorted acore_world "${DELVES}/data/sql/db-world/base"
import_tree_sorted acore_world "${MYTHIC}/Data/SQL/world"
import_tree_sorted acore_characters "${MYTHIC}/Data/SQL/characters"

mysql_exec acore_world -Nse "SELECT COUNT(*) FROM creature_template WHERE entry = 900001" |
  grep -Eq '^[1-9][0-9]*$' || fail "Mythic+ pedestal creature 900001 missing"

mysql_exec acore_world -Nse "SELECT COUNT(*) FROM instance_template WHERE map BETWEEN 900 AND 911 OR map = 805" |
  grep -Eq '^[1-9][0-9]*$' || fail "Delves instance templates missing"

mysql_exec acore_characters -Nse "SHOW TABLES LIKE 'character_mythic_keys'" |
  grep -Fxq character_mythic_keys || fail "Mythic+ character tables missing"

pass "fresh current-schema DB accepted core + selected module + playable SQL"
