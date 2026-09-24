#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ACCOUNT="${1:-fury}"
PASSWORD="${2:-fury}"
DB_PASSWORD="${FURY_MYSQL_PASSWORD:-fury}"
DB_PORT="${FURY_MYSQL_PORT:-3306}"
REALM_ADDRESS="${FURY_REALM_ADDRESS:-$(hostname -I 2>/dev/null | awk '{print $1}')}"
REALM_ADDRESS="${REALM_ADDRESS:-127.0.0.1}"

fail() { echo "[FURY][BOOTSTRAP][FAIL] $*" >&2; exit 1; }
command -v docker >/dev/null 2>&1 || fail "Docker is required for the bundled zero-config database"
docker compose version >/dev/null 2>&1 || fail "Docker Compose plugin is required"
[[ -x "${ROOT}/bin/worldserver" ]] || fail "worldserver missing"
[[ -x "${ROOT}/bin/authserver" ]] || fail "authserver missing"
[[ -d "${ROOT}/data/dbc" ]] || fail "server data missing"

cd "${ROOT}"
docker compose -f runtime/docker-compose.playable.yml up -d mysql

echo "[FURY] waiting for MySQL"
for _ in {1..90}; do
  if docker compose -f runtime/docker-compose.playable.yml exec -T mysql       mysqladmin ping -h localhost -uroot -p"${DB_PASSWORD}" --silent >/dev/null 2>&1; then
    break
  fi
  sleep 1
done
docker compose -f runtime/docker-compose.playable.yml exec -T mysql   mysqladmin ping -h localhost -uroot -p"${DB_PASSWORD}" --silent >/dev/null 2>&1   || fail "MySQL did not become healthy"

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

mkdir -p empty-lua logs etc/modules
for dist in etc/modules/*.conf.dist; do
  [[ -e "${dist}" ]] || continue
  cp -n "${dist}" "${dist%.dist}"
done

cp etc/worldserver.conf.dist etc/worldserver.bootstrap.conf
python3 - "${ROOT}/etc/worldserver.bootstrap.conf" "${ROOT}/empty-lua" <<'PY'
from pathlib import Path
import sys
conf, empty = map(Path, sys.argv[1:3])
text = conf.read_text()
text = text.replace('Eluna.ScriptPath = "lua_scripts"', f'Eluna.ScriptPath = "{empty}"')
conf.write_text(text)
PY

fifo="$(mktemp -u)"
mkfifo "${fifo}"
log="${ROOT}/logs/bootstrap-worldserver.log"
cleanup() {
  rm -f "${fifo}"
  if [[ -n "${pid:-}" ]] && kill -0 "${pid}" 2>/dev/null; then
    kill "${pid}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

echo "[FURY] first worldserver start: creating/updating AzerothCore databases"
"${ROOT}/bin/worldserver" -c "${ROOT}/etc/worldserver.bootstrap.conf" <"${fifo}" >"${log}" 2>&1 &
pid=$!
exec 3>"${fifo}"

deadline=$((SECONDS + 900))
while (( SECONDS < deadline )); do
  if grep -Fq "worldserver-daemon) ready..." "${log}"; then
    break
  fi
  if ! kill -0 "${pid}" 2>/dev/null; then
    tail -n 200 "${log}" >&2 || true
    fail "worldserver exited during database initialization"
  fi
  sleep 1
done
grep -Fq "worldserver-daemon) ready..." "${log}" || {
  tail -n 200 "${log}" >&2 || true
  fail "worldserver database initialization timed out"
}

printf 'account create %s %s\n' "${ACCOUNT}" "${PASSWORD}" >&3
sleep 2
printf 'server shutdown 1\n' >&3
exec 3>&-
wait "${pid}" || true
pid=""

mysql_exec() {
  docker compose -f runtime/docker-compose.playable.yml exec -T mysql     mysql -uroot -p"${DB_PASSWORD}" "$@"
}

echo "[FURY] importing Delves + Mythic+ content"
while IFS= read -r -d '' sql; do
  case "${sql}" in
    */world/*) db=acore_world ;;
    */characters/*) db=acore_characters ;;
    *) continue ;;
  esac
  echo "  -> ${db}: ${sql#${ROOT}/}"
  mysql_exec "${db}" < "${sql}"
done < <(find "${ROOT}/fury-sql" -type f -name '*.sql' -print0 | sort -z)

mysql_exec acore_auth -e   "UPDATE realmlist SET name='FURY Azeroth', address='${REALM_ADDRESS}', localAddress='127.0.0.1', port=8085 WHERE id=1;"

cp -n etc/worldserver.conf.dist etc/worldserver.conf
cp -n etc/authserver.conf.dist etc/authserver.conf

echo "[FURY][BOOTSTRAP][PASS] server initialized"
echo "[FURY] account: ${ACCOUNT}"
echo "[FURY] realm address: ${REALM_ADDRESS}"
echo "[FURY] next: ./runtime/run-playable.sh"
