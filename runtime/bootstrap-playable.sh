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
[[ -d "${ROOT}/runtime-source/data/sql" ]] || fail "runtime SQL source tree missing"
[[ -x "${ROOT}/runtime/mysql-wrapper.sh" ]] || fail "bundled MySQL wrapper missing"
[[ -d "${ROOT}/runtime-libs" ]] || fail "bundled runtime libraries missing"

export LD_LIBRARY_PATH="${ROOT}/runtime-libs${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

cd "${ROOT}"
docker compose -f runtime/docker-compose.playable.yml up -d mysql

echo "[FURY] waiting for MySQL"
for _ in {1..90}; do
  if docker compose -f runtime/docker-compose.playable.yml exec -T mysql \
      mysqladmin ping -h localhost -uroot -p"${DB_PASSWORD}" --silent >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

docker compose -f runtime/docker-compose.playable.yml exec -T mysql \
  mysqladmin ping -h localhost -uroot -p"${DB_PASSWORD}" --silent >/dev/null 2>&1 \
  || fail "MySQL did not become healthy"

mysql_exec() {
  docker compose -f runtime/docker-compose.playable.yml exec -T mysql \
    mysql -uroot -p"${DB_PASSWORD}" "$@"
}

progression_bracket="$(mysql_exec acore_fury -Nse \
  "SELECT state_value FROM fury_runtime_state WHERE state_key='progression_bracket' LIMIT 1;" \
  2>/dev/null || true)"
progression_bracket="${progression_bracket:-0}"

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

mkdir -p empty-lua logs etc/modules
for dist in etc/modules/*.conf.dist; do
  [[ -e "${dist}" ]] || continue
  cp -n "${dist}" "${dist%.dist}"
done

"${ROOT}/runtime/configure-llm-chatter.sh"

progression_conf="${ROOT}/etc/modules/progression_system.conf"
[[ -f "${progression_conf}" ]] || fail "Progression System config missing"
python3 - "${progression_conf}" "${progression_bracket}" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
selected = sys.argv[2]
text = path.read_text()

pattern = re.compile(
    r'(?m)^(ProgressionSystem\.Bracket_([A-Za-z0-9_]+)\s*=\s*)[01]\s*$'
)
available = {match.group(2) for match in pattern.finditer(text)}
if selected not in available:
    raise SystemExit(
        f"[FURY][BOOTSTRAP][FAIL] persisted progression bracket is invalid: {selected}"
    )

text = pattern.sub(
    lambda match: match.group(1) + ("1" if match.group(2) == selected else "0"),
    text,
)
path.write_text(text)
print(f"[FURY] progression bracket selected: {selected}")
PY

"${ROOT}/runtime/configure-playable-modules.sh" "${progression_bracket}"

cp etc/worldserver.conf.dist etc/worldserver.bootstrap.conf
python3 - "${ROOT}/etc/worldserver.bootstrap.conf" "${ROOT}/empty-lua" "${ROOT}/runtime-source" "${ROOT}/runtime/mysql-wrapper.sh" "${ROOT}/data" <<'PY'
from pathlib import Path
import re
import sys

conf, empty, source, mysql_wrapper, data_dir = map(Path, sys.argv[1:6])
text = conf.read_text()

def set_option(payload: str, key: str, value: str) -> str:
    line = f'{key} = "{value}"'
    pattern = re.compile(rf'(?m)^\s*{re.escape(key)}\s*=.*$')
    if pattern.search(payload):
        return pattern.sub(line, payload, count=1)
    return payload.rstrip() + "\n" + line + "\n"

text = set_option(text, "Eluna.ScriptPath", str(empty))
text = set_option(text, "SourceDirectory", str(source))
text = set_option(text, "MySQLExecutable", str(mysql_wrapper))
text = set_option(text, "DataDir", str(data_dir))
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

mysql_exec acore_fury -e "
  CREATE TABLE IF NOT EXISTS fury_runtime_state (
    state_key VARCHAR(96) NOT NULL PRIMARY KEY,
    state_value VARCHAR(255) NOT NULL,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
  ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
"

mysql_exec acore_fury -e "
  INSERT IGNORE INTO fury_runtime_state (state_key, state_value)
  VALUES ('progression_bracket', '${progression_bracket}');
"

content_marker="$(mysql_exec acore_fury -Nse \
  "SELECT state_value FROM fury_runtime_state WHERE state_key='playable_content_v1' LIMIT 1;")"

if [[ "${content_marker}" != "applied" ]]; then
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

  mysql_exec acore_fury -e "
    INSERT INTO fury_runtime_state (state_key, state_value)
    VALUES ('playable_content_v1', 'applied')
    ON DUPLICATE KEY UPDATE state_value=VALUES(state_value);
  "
else
  echo "[FURY] Delves + Mythic+ playable content v1 already applied; skipping one-time import"
fi

mysql_exec acore_auth -e \
  "UPDATE realmlist SET name='FURY Azeroth', address='${REALM_ADDRESS}', localAddress='127.0.0.1', port=8085 WHERE id=1;"

cp -n etc/worldserver.conf.dist etc/worldserver.conf
cp -n etc/authserver.conf.dist etc/authserver.conf

python3 - "${ROOT}/etc/worldserver.conf" "${ROOT}/etc/authserver.conf" "${ROOT}/runtime-source" "${ROOT}/runtime/mysql-wrapper.sh" "${ROOT}/data" <<'PY'
from pathlib import Path
import re
import sys

world, auth, source, mysql_wrapper, data_dir = map(Path, sys.argv[1:6])

def set_option(payload: str, key: str, value: str) -> str:
    line = f'{key} = "{value}"'
    pattern = re.compile(rf'(?m)^\s*{re.escape(key)}\s*=.*$')
    if pattern.search(payload):
        return pattern.sub(line, payload, count=1)
    return payload.rstrip() + "\n" + line + "\n"

world_text = world.read_text()
world_text = set_option(world_text, "SourceDirectory", str(source))
world_text = set_option(world_text, "MySQLExecutable", str(mysql_wrapper))
world_text = set_option(world_text, "DataDir", str(data_dir))
world.write_text(world_text)

auth_text = auth.read_text()
auth_text = set_option(auth_text, "SourceDirectory", str(source))
auth_text = set_option(auth_text, "MySQLExecutable", str(mysql_wrapper))
auth.write_text(auth_text)
PY

account_sql="$(python3 - "${ACCOUNT}" <<'PY'
import sys

value = sys.argv[1].encode("utf-8").hex()
print(f"SELECT COUNT(*) FROM account WHERE username=UPPER(CONVERT(0x{value} USING utf8mb4));")
PY
)"
account_count="$(mysql_exec acore_auth -Nse "${account_sql}")"
[[ "${account_count}" =~ ^[1-9][0-9]*$ ]] || fail "game account was not created"

echo "[FURY][BOOTSTRAP][PASS] server initialized"
echo "[FURY] account: ${ACCOUNT}"
echo "[FURY] realm address: ${REALM_ADDRESS}"
echo "[FURY] progression bracket: ${progression_bracket}"
echo "[FURY] next: ./runtime/run-playable.sh"
