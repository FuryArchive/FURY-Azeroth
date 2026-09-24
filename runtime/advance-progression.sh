#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PASSWORD="${FURY_MYSQL_PASSWORD:-fury}"
DB_PORT="${FURY_MYSQL_PORT:-3306}"
COMPOSE="${ROOT}/runtime/docker-compose.playable.yml"
CONF="${ROOT}/etc/modules/progression_system.conf"
LOG="${ROOT}/logs/progression-advance.log"

fail() { echo "[FURY][PROGRESSION][FAIL] $*" >&2; exit 1; }

command -v docker >/dev/null 2>&1 || fail "Docker is required"
docker compose version >/dev/null 2>&1 || fail "Docker Compose plugin is required"
[[ -x "${ROOT}/bin/worldserver" ]] || fail "worldserver missing"
[[ -f "${ROOT}/etc/worldserver.conf" ]] || fail "server is not bootstrapped"
[[ -f "${CONF}" ]] || fail "Progression System config missing"
[[ -d "${ROOT}/runtime-libs" ]] || fail "bundled runtime libraries missing"

if pgrep -x worldserver >/dev/null 2>&1 || pgrep -x authserver >/dev/null 2>&1; then
  fail "stop the realm before advancing progression"
fi

export LD_LIBRARY_PATH="${ROOT}/runtime-libs${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

cd "${ROOT}"
docker compose -f "${COMPOSE}" up -d mysql

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

mysql_exec() {
  docker compose -f "${COMPOSE}" exec -T mysql \
    mysql -uroot -p"${DB_PASSWORD}" "$@"
}

current="$(mysql_exec acore_fury -Nse \
  "SELECT state_value FROM fury_runtime_state WHERE state_key='progression_bracket' LIMIT 1;")"
[[ -n "${current}" ]] || fail "progression state is missing; run bootstrap first"

# Normalize FURY's playable module policy and repair the upstream-omitted
# 70_6_3 config key before deriving the next sequential bracket.
"${ROOT}/runtime/configure-playable-modules.sh" "${current}"

next="$(python3 - "${CONF}" "${current}" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
current = sys.argv[2]
text = path.read_text()
pattern = re.compile(
    r'(?m)^ProgressionSystem\.Bracket_([A-Za-z0-9_]+)\s*=\s*[01]\s*$'
)
order = [m.group(1) for m in pattern.finditer(text) if m.group(1) != "Custom"]
if current not in order:
    raise SystemExit(f"unknown persisted bracket: {current}")
index = order.index(current)
print("__END__" if index + 1 >= len(order) else order[index + 1])
PY
)"

if [[ "${next}" == "__END__" ]]; then
  echo "[FURY][PROGRESSION][PASS] already at final WotLK progression bracket: ${current}"
  exit 0
fi

echo "[FURY] progression: ${current} -> ${next}"
echo "[FURY] creating safety backup before progression migration"
"${ROOT}/runtime/backup-playable.sh"

python3 - "${CONF}" "${next}" <<'PY'
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
    raise SystemExit(f"unknown target bracket: {selected}")
text = pattern.sub(
    lambda match: match.group(1) + ("1" if match.group(2) == selected else "0"),
    text,
)
path.write_text(text)
PY

"${ROOT}/runtime/configure-playable-modules.sh" "${next}"

export AC_LOGIN_DATABASE_INFO="127.0.0.1;${DB_PORT};root;${DB_PASSWORD};acore_auth"
export AC_WORLD_DATABASE_INFO="127.0.0.1;${DB_PORT};root;${DB_PASSWORD};acore_world"
export AC_CHARACTER_DATABASE_INFO="127.0.0.1;${DB_PORT};root;${DB_PASSWORD};acore_characters"
export AC_UPDATES_ENABLE_DATABASES="7"
export AC_UPDATES_AUTO_SETUP="1"
export AC_DISABLE_INTERACTIVE="1"
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

mkdir -p "${ROOT}/logs"
fifo="$(mktemp -u)"
mkfifo "${fifo}"
pid=""

cleanup() {
  rm -f "${fifo}"
  if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
    kill "${pid}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

echo "[FURY] applying progression bracket ${next}"
"${ROOT}/bin/worldserver" -c "${ROOT}/etc/worldserver.conf" <"${fifo}" >"${LOG}" 2>&1 &
pid=$!
exec 3>"${fifo}"

deadline=$((SECONDS + 900))
while (( SECONDS < deadline )); do
  if grep -Fq "worldserver-daemon) ready..." "${LOG}"; then
    break
  fi
  if ! kill -0 "${pid}" 2>/dev/null; then
    tail -n 200 "${LOG}" >&2 || true
    fail "worldserver exited while applying progression bracket ${next}"
  fi
  sleep 1
done

grep -Fq "worldserver-daemon) ready..." "${LOG}" || {
  tail -n 200 "${LOG}" >&2 || true
  fail "progression migration timed out"
}

printf 'server shutdown 1\n' >&3
exec 3>&-
wait "${pid}" || true
pid=""

mysql_exec acore_fury -e "
  UPDATE fury_runtime_state
  SET state_value='${next}'
  WHERE state_key='progression_bracket';
"

echo "[FURY][PROGRESSION][PASS] advanced ${current} -> ${next}"
echo "[FURY] next: ./runtime/run-playable.sh"
