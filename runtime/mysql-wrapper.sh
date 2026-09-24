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
for arg in "$@"; do
  case "${arg}" in
    --defaults-extra-file=*)
      # AzerothCore writes host-side credentials here. The mysql client runs
      # inside the bundled database container, so use the container password.
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
