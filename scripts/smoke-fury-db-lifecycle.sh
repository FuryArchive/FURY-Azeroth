#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORLDSERVER="${FURY_WORLDSERVER:-${ROOT}/build/dist/bin/worldserver}"
CONF_DIR="${FURY_CONF_DIR:-${ROOT}/build/dist/etc}"
WORLDSERVER_CONF="${FURY_WORLDSERVER_CONF:-${CONF_DIR}/worldserver.conf.dist}"
STARTUP_TIMEOUT="${FURY_DB_STARTUP_TIMEOUT:-600}"

MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
MYSQL_PORT="${MYSQL_PORT:-3306}"
MYSQL_USER="${MYSQL_USER:-root}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-fury}"

if [[ ! -x "${WORLDSERVER}" ]]; then
  echo "[FURY][FAIL] worldserver not found: ${WORLDSERVER}" >&2
  exit 2
fi

if [[ ! -f "${WORLDSERVER_CONF}" ]]; then
  echo "[FURY][FAIL] worldserver config template missing: ${WORLDSERVER_CONF}" >&2
  exit 2
fi

export MYSQL_PWD="${MYSQL_PASSWORD}"
mysql_cmd=(
  mysql
  --protocol=tcp
  --host="${MYSQL_HOST}"
  --port="${MYSQL_PORT}"
  --user="${MYSQL_USER}"
  --batch
  --skip-column-names
)

sql_global() {
  "${mysql_cmd[@]}" -e "$1"
}

sql_fury() {
  "${mysql_cmd[@]}" acore_fury -e "$1"
}

assert_eq() {
  local expected="$1"
  local actual="$2"
  local label="$3"

  if [[ "${actual}" != "${expected}" ]]; then
    echo "[FURY][FAIL] ${label}: expected=${expected} actual=${actual}" >&2
    exit 1
  fi

  echo "[FURY][PASS] ${label}"
}

# AzerothCore installs module templates as *.conf.dist but the runtime loader
# asks for the corresponding *.conf names. Materialize CI-only copies.
mkdir -p "${CONF_DIR}/modules"
shopt -s nullglob
for dist in "${CONF_DIR}/modules/"*.conf.dist; do
  cp -f "${dist}" "${dist%.dist}"
done
shopt -u nullglob

db_info() {
  local database="$1"
  printf '%s;%s;%s;%s;%s'     "${MYSQL_HOST}" "${MYSQL_PORT}" "${MYSQL_USER}" "${MYSQL_PASSWORD}" "${database}"
}

# ConfigMgr maps these AC_* variables onto the canonical config keys.
# Core DBs stay on the exact pinned updater path; no hand-written fake schema.
export AC_LOGIN_DATABASE_INFO="$(db_info acore_auth)"
export AC_WORLD_DATABASE_INFO="$(db_info acore_world)"
export AC_CHARACTER_DATABASE_INFO="$(db_info acore_characters)"
export AC_UPDATES_ENABLE_DATABASES=7
export AC_UPDATES_AUTO_SETUP=1
export AC_PLAYERBOTS_DATABASE_INFO="$(db_info acore_playerbots)"
export AC_PLAYERBOTS_UPDATES_ENABLE_DATABASES=1
export AC_FURY_ENABLE=1
export AC_FURY_DATABASE_INFO="$(db_info acore_fury)"
export AC_FURY_UPDATES_ENABLE_DATABASES=1
export AC_FURY_DATABASE_SOURCE_DIRECTORY="${ROOT}/modules/mod-fury"
export AC_CONSOLE_ENABLE=0
export AC_BEEP_AT_START=0
export AC_LOG_ASYNC_ENABLE=0

echo "[FURY] resetting CI integration databases"
sql_global "
  DROP DATABASE IF EXISTS acore_fury;
  DROP DATABASE IF EXISTS acore_playerbots;
  DROP DATABASE IF EXISTS acore_characters;
  DROP DATABASE IF EXISTS acore_world;
  DROP DATABASE IF EXISTS acore_auth;
"

run_until_fury_disabled() (
  local workdir log pid=""
  workdir="$(mktemp -d)"
  log="${workdir}/worldserver.log"

  cleanup() {
    if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
      kill -TERM "${pid}" 2>/dev/null || true
      local deadline=$((SECONDS + 15))
      while kill -0 "${pid}" 2>/dev/null && (( SECONDS < deadline )); do
        sleep 1
      done
      if kill -0 "${pid}" 2>/dev/null; then
        kill -KILL "${pid}" 2>/dev/null || true
      fi
      wait "${pid}" 2>/dev/null || true
    fi
    rm -rf "${workdir}"
  }
  trap cleanup EXIT

  echo "[FURY] disabled-mode worldserver database startup"
  "${WORLDSERVER}" -c "${WORLDSERVER_CONF}" </dev/null >"${log}" 2>&1 &
  pid=$!

  local deadline=$((SECONDS + STARTUP_TIMEOUT))
  while (( SECONDS < deadline )); do
    if grep -Fq "[FURY] module disabled; skipping FURY database startup." "${log}"; then
      echo "[FURY][PASS] disabled mode skips the FURY database lifecycle"
      return 0
    fi

    if ! kill -0 "${pid}" 2>/dev/null; then
      echo "[FURY][FAIL] disabled-mode worldserver exited before FURY skip marker" >&2
      tail -n 250 "${log}" >&2 || true
      return 1
    fi

    sleep 1
  done

  echo "[FURY][FAIL] timeout waiting for disabled FURY database skip marker" >&2
  tail -n 250 "${log}" >&2 || true
  return 1
)

run_until_fury_db_ready() (
  local run_number="$1"
  local workdir log pid=""
  workdir="$(mktemp -d)"
  log="${workdir}/worldserver.log"

  cleanup() {
    if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
      kill -TERM "${pid}" 2>/dev/null || true
      sleep 1
      if kill -0 "${pid}" 2>/dev/null; then
        kill -KILL "${pid}" 2>/dev/null || true
      fi
      wait "${pid}" 2>/dev/null || true
    fi
    rm -rf "${workdir}"
  }
  trap cleanup EXIT

  echo "[FURY] DB lifecycle worldserver run ${run_number}/2"
  "${WORLDSERVER}" -c "${WORLDSERVER_CONF}" </dev/null >"${log}" 2>&1 &
  pid=$!

  local deadline=$((SECONDS + STARTUP_TIMEOUT))
  while (( SECONDS < deadline )); do
    if grep -Fq "[FURY] FURY database ready." "${log}"; then
      echo "[FURY][PASS] run ${run_number}: real FuryDatabaseScript reached ready state"
      return 0
    fi

    if ! kill -0 "${pid}" 2>/dev/null; then
      echo "[FURY][FAIL] run ${run_number}: worldserver exited before FURY DB ready" >&2
      tail -n 250 "${log}" >&2 || true
      return 1
    fi

    sleep 1
  done

  echo "[FURY][FAIL] run ${run_number}: timeout waiting for FURY DB ready" >&2
  tail -n 250 "${log}" >&2 || true
  return 1
)

export AC_FURY_ENABLE=0
run_until_fury_disabled
assert_eq "0" "$(sql_global "SELECT COUNT(*) FROM information_schema.schemata WHERE schema_name='acore_fury';")"   "disabled mode does not create acore_fury"

export AC_FURY_ENABLE=1
run_until_fury_db_ready 1

assert_eq "1" "$(sql_global "SELECT COUNT(*) FROM information_schema.schemata WHERE schema_name='acore_fury';")"   "FURY database auto-created by worldserver"
assert_eq "1" "$(sql_fury "SELECT COUNT(*) FROM version_db_fury WHERE sql_rev='2026_09_22_00_fury_base';")"   "FURY base revision populated once"

sql_fury "
  INSERT INTO fury_household (slug, display_name)
  VALUES ('ci-restart-sentinel', 'CI restart sentinel')
  ON DUPLICATE KEY UPDATE display_name=VALUES(display_name);
"

run_until_fury_db_ready 2

assert_eq "1" "$(sql_fury "SELECT COUNT(*) FROM fury_household WHERE slug='ci-restart-sentinel';")"   "second worldserver start preserves FURY data"
assert_eq "1" "$(sql_fury "SELECT COUNT(*) FROM version_db_fury WHERE sql_rev='2026_09_22_00_fury_base';")"   "second worldserver start does not duplicate base revision"

echo "[FURY][PASS] real module database create/populate/update/restart lifecycle passed"
