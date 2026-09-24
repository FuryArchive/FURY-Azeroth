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
      # AzerothCore writes host-side credentials here. The MySQL client runs
      # inside our container, so that temp path is intentionally replaced by
      # the password already used to launch the bundled MySQL service.
      ;;
    *)
      filtered+=("${arg}")
      ;;
  esac
done

exec docker compose -f "${COMPOSE}" exec -T mysql   mysql "-p${DB_PASSWORD}" "${filtered[@]}"
