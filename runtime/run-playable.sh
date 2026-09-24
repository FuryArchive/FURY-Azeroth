#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PASSWORD="${FURY_MYSQL_PASSWORD:-fury}"
DB_PORT="${FURY_MYSQL_PORT:-3306}"
COMPOSE="${ROOT}/runtime/docker-compose.playable.yml"

fail() { echo "[FURY][RUN][FAIL] $*" >&2; exit 1; }

command -v docker >/dev/null 2>&1 || fail "Docker is required"
docker compose version >/dev/null 2>&1 || fail "Docker Compose plugin is required"

[[ -f "${ROOT}/etc/worldserver.conf" && -f "${ROOT}/etc/authserver.conf" ]] ||   fail "server is not bootstrapped yet; run ./runtime/bootstrap-playable.sh first"
[[ -d "${ROOT}/runtime-source/data/sql" ]] || fail "runtime SQL source tree is missing"
[[ -d "${ROOT}/runtime-libs" ]] || fail "bundled runtime libraries are missing"

export LD_LIBRARY_PATH="${ROOT}/runtime-libs${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

cd "${ROOT}"
docker compose -f "${COMPOSE}" up -d mysql

echo "[FURY] waiting for MySQL"
for _ in {1..90}; do
  if docker compose -f "${COMPOSE}" exec -T mysql \
      mysqladmin ping -h localhost -uroot -p"${DB_PASSWORD}" --silent >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

docker compose -f "${COMPOSE}" exec -T mysql \
  mysqladmin ping -h localhost -uroot -p"${DB_PASSWORD}" --silent >/dev/null 2>&1 \
  || fail "MySQL did not become healthy"

bridge_started=0
llm_conf="${ROOT}/etc/modules/mod_llm_chatter.conf"
if [[ -f "${llm_conf}" ]] && grep -Eq '^LLMChatter\.Enable[[:space:]]*=[[:space:]]*1[[:space:]]*$' "${llm_conf}"; then
  if [[ -f "${ROOT}/llm-chatter/llm_chatter_bridge.py" && -d "${ROOT}/llm-chatter/wheels" ]]; then
    echo "[FURY] starting packaged LLM Chatter bridge"
    if docker compose -f "${COMPOSE}" --profile llm up -d llm-bridge; then
      bridge_started=1
    else
      echo "[FURY][LLM][WARN] bridge failed to start; realm will continue without generated chatter" >&2
    fi
  else
    echo "[FURY][LLM][WARN] chatter is enabled but packaged bridge files are missing" >&2
  fi
else
  echo "[FURY] LLM Chatter disabled; realm startup does not depend on an LLM"
fi

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
  kill "${auth_pid:-}" 2>/dev/null || true
  wait "${auth_pid:-}" 2>/dev/null || true
  if [[ "${bridge_started}" -eq 1 ]]; then
    docker compose -f "${COMPOSE}" --profile llm stop llm-bridge >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT INT TERM

echo "[FURY] authserver pid=${auth_pid}"
[[ "${bridge_started}" -eq 0 ]] || echo "[FURY] LLM Chatter bridge is running"
echo "[FURY] worldserver console follows; Ctrl+C stops the realm"
"${ROOT}/bin/worldserver" -c "${ROOT}/etc/worldserver.conf"
