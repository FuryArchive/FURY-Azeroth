#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATA_DIR="${FURY_SERVER_DATA_DIR:-${ROOT}/build/server-data}"
CONF_DIR="${FURY_CONF_DIR:-${ROOT}/build/dist/etc}"

MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
MYSQL_PORT="${MYSQL_PORT:-3306}"
MYSQL_USER="${MYSQL_USER:-root}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-fury}"

for required in dbc maps vmaps; do
  if [[ ! -d "${DATA_DIR}/${required}" ]]; then
    echo "[FURY][FAIL] missing server-data directory: ${DATA_DIR}/${required}" >&2
    exit 2
  fi
done

mkdir -p "${CONF_DIR}/modules"
shopt -s nullglob
for dist in "${CONF_DIR}/modules/"*.conf.dist; do
  cp -f "${dist}" "${dist%.dist}"
done
shopt -u nullglob

db_info() {
  local database="$1"
  printf '%s;%s;%s;%s;%s' \
    "${MYSQL_HOST}" "${MYSQL_PORT}" "${MYSQL_USER}" "${MYSQL_PASSWORD}" "${database}"
}

export AC_LOGIN_DATABASE_INFO="$(db_info acore_auth)"
export AC_WORLD_DATABASE_INFO="$(db_info acore_world)"
export AC_CHARACTER_DATABASE_INFO="$(db_info acore_characters)"
export AC_PLAYERBOTS_DATABASE_INFO="$(db_info acore_playerbots)"
export AC_FURY_DATABASE_INFO="$(db_info acore_fury)"
export AC_FURY_ENABLE=1
export AC_FURY_UPDATES_ENABLE_DATABASES=1
unset AC_FURY_DATABASE_SOURCE_DIRECTORY || true
export AC_DATA_DIR="${DATA_DIR}"
export AC_BEEP_AT_START=0
export AC_LOG_ASYNC_ENABLE=0

export FURY_WORLDSERVER_CONF="${FURY_WORLDSERVER_CONF:-${CONF_DIR}/worldserver.conf.dist}"
export FURY_STARTUP_TIMEOUT="${FURY_STARTUP_TIMEOUT:-600}"
export FURY_SMOKE_RUNS="${FURY_SMOKE_RUNS:-2}"

exec bash "${ROOT}/scripts/smoke-worldserver.sh"
