#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PASSWORD="${FURY_MYSQL_PASSWORD:-fury}"
BACKUP_DIR="${1:-${ROOT}/backups}"
COMPOSE="${ROOT}/runtime/docker-compose.playable.yml"

fail() { echo "[FURY][BACKUP][FAIL] $*" >&2; exit 1; }

command -v docker >/dev/null 2>&1 || fail "Docker is required"
docker compose version >/dev/null 2>&1 || fail "Docker Compose plugin is required"

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

mkdir -p "${BACKUP_DIR}"
stamp="$(date +%Y%m%d-%H%M%S)"
work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

mapfile -t databases < <(
  docker compose -f "${COMPOSE}" exec -T mysql \
    mysql -uroot -p"${DB_PASSWORD}" -Nse \
    "SHOW DATABASES WHERE \`Database\` NOT IN ('information_schema','mysql','performance_schema','sys');"
)

[[ "${#databases[@]}" -gt 0 ]] || fail "no playable databases found"

for db in "${databases[@]}"; do
  [[ "${db}" =~ ^[A-Za-z0-9_]+$ ]] || fail "unexpected database name: ${db}"
  echo "[FURY] backing up ${db}"
  docker compose -f "${COMPOSE}" exec -T mysql \
    mysqldump -uroot -p"${DB_PASSWORD}" \
      --single-transaction --quick --routines --events --triggers \
      --databases "${db}" --add-drop-database \
      > "${work}/${db}.sql"
done

cat > "${work}/BACKUP-MANIFEST.txt" <<EOF
created_at=$(date --iso-8601=seconds)
database_count=${#databases[@]}
databases=${databases[*]}
EOF

archive="${BACKUP_DIR}/FURY-Azeroth-backup-${stamp}.tar.gz"
tar -C "${work}" -czf "${archive}" .

echo "[FURY][BACKUP][PASS] ${archive}"
