#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARCHIVE="${1:-}"
DB_PASSWORD="${FURY_MYSQL_PASSWORD:-fury}"
COMPOSE="${ROOT}/runtime/docker-compose.playable.yml"

fail() { echo "[FURY][RESTORE][FAIL] $*" >&2; exit 1; }

[[ -n "${ARCHIVE}" ]] || fail "usage: ./runtime/restore-playable.sh /path/to/FURY-Azeroth-backup-*.tar.gz"
[[ -f "${ARCHIVE}" ]] || fail "backup archive not found: ${ARCHIVE}"
command -v docker >/dev/null 2>&1 || fail "Docker is required"
docker compose version >/dev/null 2>&1 || fail "Docker Compose plugin is required"

if pgrep -f "${ROOT}/bin/worldserver" >/dev/null 2>&1 || pgrep -f "${ROOT}/bin/authserver" >/dev/null 2>&1; then
  fail "authserver/worldserver is running; stop the realm before restoring"
fi

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
tar -xzf "${ARCHIVE}" -C "${work}"
[[ -f "${work}/BACKUP-MANIFEST.txt" ]] || fail "invalid backup: manifest missing"

mapfile -t dumps < <(find "${work}" -maxdepth 1 -type f -name '*.sql' -print | sort)
[[ "${#dumps[@]}" -gt 0 ]] || fail "invalid backup: no SQL dumps found"

cd "${ROOT}"
docker compose -f "${COMPOSE}" up -d mysql

for _ in {1..90}; do
  if docker compose -f "${COMPOSE}" exec -T mysql       mysqladmin ping -h localhost -uroot -p"${DB_PASSWORD}" --silent >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

docker compose -f "${COMPOSE}" exec -T mysql   mysqladmin ping -h localhost -uroot -p"${DB_PASSWORD}" --silent >/dev/null 2>&1   || fail "MySQL did not become healthy"

echo "[FURY] restoring backup:"
cat "${work}/BACKUP-MANIFEST.txt"

for dump in "${dumps[@]}"; do
  echo "[FURY] restoring $(basename "${dump}")"
  docker compose -f "${COMPOSE}" exec -T mysql     mysql -uroot -p"${DB_PASSWORD}" < "${dump}"
done

echo "[FURY][RESTORE][PASS] backup restored"
echo "[FURY] next: ./runtime/run-playable.sh"
