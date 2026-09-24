#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PASSWORD="${FURY_MYSQL_PASSWORD:-fury}"
DB_PORT="${FURY_MYSQL_PORT:-3306}"

command -v docker >/dev/null 2>&1 || { echo "[FURY][FAIL] Docker is required" >&2; exit 1; }
[[ -f "${ROOT}/etc/worldserver.conf" && -f "${ROOT}/etc/authserver.conf" ]] || {
  echo "[FURY][FAIL] server is not bootstrapped yet; run ./runtime/bootstrap-playable.sh first" >&2
  exit 1
}
[[ -d "${ROOT}/runtime-source/data/sql" ]] || {
  echo "[FURY][FAIL] runtime SQL source tree is missing" >&2
  exit 1
}
cd "${ROOT}"
docker compose -f runtime/docker-compose.playable.yml up -d mysql

export AC_LOGIN_DATABASE_INFO="127.0.0.1;${DB_PORT};root;${DB_PASSWORD};acore_auth"
export AC_WORLD_DATABASE_INFO="127.0.0.1;${DB_PORT};root;${DB_PASSWORD};acore_world"
export AC_CHARACTER_DATABASE_INFO="127.0.0.1;${DB_PORT};root;${DB_PASSWORD};acore_characters"
export AC_UPDATES_ENABLE_DATABASES="7"
export AC_UPDATES_AUTO_SETUP="1"
export AC_PLAYERBOTS_DATABASE_INFO="127.0.0.1;${DB_PORT};root;${DB_PASSWORD};acore_playerbots"
export AC_PLAYERBOTS_UPDATES_ENABLE_DATABASES="1"
export AC_FURY_ENABLE="1"
export AC_FURY_DATABASE_INFO="127.0.0.1;${DB_PORT};root;${DB_PASSWORD};acore_fury"
export AC_FURY_UPDATES_ENABLE_DATABASES="1"
export AC_FURY_DATABASE_SOURCE_DIRECTORY="${ROOT}/fury-module"
export AC_CONSOLE_ENABLE="1"
export AC_BEEP_AT_START="0"
export AC_LOG_ASYNC_ENABLE="0"
export AC_DATA_DIR="${ROOT}/data"

mkdir -p logs
"${ROOT}/bin/authserver" -c "${ROOT}/etc/authserver.conf" >"${ROOT}/logs/authserver.log" 2>&1 &
auth_pid=$!

cleanup() {
  kill "${auth_pid}" 2>/dev/null || true
  wait "${auth_pid}" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "[FURY] authserver pid=${auth_pid}"
echo "[FURY] worldserver console follows; Ctrl+C stops the realm"
"${ROOT}/bin/worldserver" -c "${ROOT}/etc/worldserver.conf"
