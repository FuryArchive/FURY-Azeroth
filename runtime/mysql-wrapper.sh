#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PASSWORD="${FURY_MYSQL_PASSWORD:-fury}"
COMPOSE="${ROOT}/runtime/docker-compose.playable.yml"

command -v docker >/dev/null 2>&1 || {
  echo "[FURY][MYSQL][FAIL] Docker is required" >&2
  exit 127
}

filtered=()
skip_next=0
for arg in "$@"; do
  if [[ "${skip_next}" -eq 1 ]]; then
    skip_next=0
    continue
  fi
  case "${arg}" in
    --defaults-extra-file=*)
      # AzerothCore writes host-side credentials here. The mysql client runs
      # inside the bundled database container, so use the container password.
      ;;
    -h|-P|-S|--host|--port|--socket)
      # Two-argument endpoint options: also drop their following value.
      skip_next=1
      ;;
    -h*|-P*|--host=*|--port=*|--protocol=*|-S*|--socket=*)
      # AzerothCore passes the host-side database endpoint. Inside the MySQL
      # container the server is always local on TCP 3306, regardless of the
      # host's FURY_MYSQL_PORT mapping.
      ;;
    *)
      filtered+=("${arg}")
      ;;
  esac
done

exec docker compose -f "${COMPOSE}" exec -T mysql \
  mysql "-p${DB_PASSWORD}" -h127.0.0.1 -P3306 --protocol=TCP "${filtered[@]}"
