#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="${ROOT}/upstream/azerothcore-wotlk"

MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
MYSQL_PORT="${MYSQL_PORT:-3306}"
MYSQL_USER="${MYSQL_USER:-root}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-fury}"

mysql_args=(--protocol=tcp --host="${MYSQL_HOST}" --port="${MYSQL_PORT}" --user="${MYSQL_USER}")
if [[ -n "${MYSQL_PASSWORD}" ]]; then
  mysql_args+=(--password="${MYSQL_PASSWORD}")
fi

fail() {
  echo "[FURY][DB-PREFLIGHT][FAIL] $*" >&2
  exit 1
}

mysql_exec() {
  mysql "${mysql_args[@]}" "$@"
}

import_file() {
  local db="$1"
  local sql="$2"
  echo "[FURY][DB-PREFLIGHT] ${db} <- ${sql#${ROOT}/}"
  {
    printf '%s\n' "SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;"
    cat "${sql}"
  } | mysql_exec "${db}"
}

import_file_force() {
  local db="$1"
  local sql="$2"
  echo "[FURY][DB-PREFLIGHT][CORE-UPDATE] ${db} <- ${sql#${ROOT}/}"
  {
    printf '%s\n' "SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;"
    cat "${sql}"
  } | mysql "${mysql_args[@]}" --force "${db}"
}

import_tree() {
  local db="$1"
  local dir="$2"
  [[ -d "${dir}" ]] || return 0

  while IFS= read -r -d '' sql; do
    import_file "${db}" "${sql}"
  done < <(find -L "${dir}" -type f -name '*.sql' -print0 | sort -z)
}

classify_db() {
  local path="$1"
  case "${path}" in
    */db-world/*|*/db_world/*) echo acore_world ;;
    */db-characters/*|*/db_characters/*) echo acore_characters ;;
    */db-auth/*|*/db_auth/*) echo acore_auth ;;
    */db-playerbots/*|*/db_playerbots/*) echo acore_playerbots ;;
    */db-fury/*|*/db_fury/*) echo acore_fury ;;
    *) return 1 ;;
  esac
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

[[ -d "${CORE}/data/sql/base" ]] || fail "synced AzerothCore tree missing"

for _ in {1..60}; do
  if mysql_exec -e "SELECT 1" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done
mysql_exec -e "SELECT 1" >/dev/null 2>&1 || fail "MySQL is unavailable"

mysql_exec -e "
  CREATE DATABASE IF NOT EXISTS acore_auth CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
  CREATE DATABASE IF NOT EXISTS acore_world CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
  CREATE DATABASE IF NOT EXISTS acore_characters CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
  CREATE DATABASE IF NOT EXISTS acore_playerbots CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
  CREATE DATABASE IF NOT EXISTS acore_fury CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
"

echo "[FURY][DB-PREFLIGHT] importing current AzerothCore base databases"
import_tree acore_auth "${CORE}/data/sql/base/db_auth"
import_tree acore_characters "${CORE}/data/sql/base/db_characters"
import_tree acore_world "${CORE}/data/sql/base/db_world"

echo "[FURY][DB-PREFLIGHT] replaying AzerothCore updates in schema-evolution mode"
for spec in \
  "acore_auth:${CORE}/data/sql/updates/db_auth" \
  "acore_characters:${CORE}/data/sql/updates/db_characters" \
  "acore_world:${CORE}/data/sql/updates/db_world"
do
  db="${spec%%:*}"
  dir="${spec#*:}"
  [[ -d "${dir}" ]] || continue
  while IFS= read -r -d '' sql; do
    import_file_force "${db}" "${sql}"
  done < <(find "${dir}" -maxdepth 1 -type f -name '*.sql' -print0 | sort -z)
done

mysql_exec acore_world -Nse "SHOW COLUMNS FROM creature LIKE 'id'" | grep -q . \
  || fail "core update replay did not produce current creature.id schema"
mysql_exec acore_world -Nse "SHOW COLUMNS FROM creature LIKE 'id1'" | grep -q . \
  && fail "core update replay left obsolete creature.id1 schema"

echo "[FURY][DB-PREFLIGHT] applying selected module SQL without compiling C++"
while IFS= read -r -d '' sql; do
  case "${sql}" in
    */archive/*|*/old/*) continue ;;
  esac

  db="$(classify_db "${sql}" || true)"
  [[ -n "${db}" ]] || continue
  import_file "${db}" "${sql}"
done < <(find -L "${CORE}/modules" -type f -path '*/data/sql/*' -name '*.sql' -print0 | sort -z)

DELVES="${ROOT}/upstream/integrations/$(read_integration_dir delves)"
MYTHIC="${ROOT}/upstream/integrations/$(read_integration_dir mythic_plus_extended)"

echo "[FURY][DB-PREFLIGHT] applying playable integration SQL"
import_tree acore_world "${DELVES}/data/sql/db-world/base"
import_tree acore_world "${MYTHIC}/Data/SQL/world"
import_tree acore_characters "${MYTHIC}/Data/SQL/characters"

mysql_exec acore_world -Nse "SHOW COLUMNS FROM item_template LIKE 'StatsCount'" | grep -q .   && fail "current item_template unexpectedly still exposes StatsCount"

mysql_exec acore_world -Nse "SELECT COUNT(*) FROM playercreateinfo WHERE race IN (9,12)" | grep -Eq '^[1-9][0-9]*$'   || fail "Worgen/Goblin playercreateinfo rows were not installed"

mysql_exec acore_characters -Nse "SHOW TABLES LIKE 'character_nemesis'" | grep -Fxq 'character_nemesis'   || fail "Nemesis character table missing"

echo "[FURY][DB-PREFLIGHT][PASS] clean current-schema database accepted selected module + playable SQL"
