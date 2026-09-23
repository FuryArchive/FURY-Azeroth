#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "${WORKDIR}"' EXIT

cat >"${WORKDIR}/Define.h" <<'EOF'
#pragma once
#include <cstdint>
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
EOF

cat >"${WORKDIR}/ObjectGuid.h" <<'EOF'
#pragma once
class ObjectGuid
{
public:
    static ObjectGuid const Empty;
    bool IsEmpty() const { return true; }
    unsigned long long GetRawValue() const { return 0; }
};
inline ObjectGuid const ObjectGuid::Empty{};
EOF

CXX="${CXX:-g++}"

"${CXX}" \
  -std=c++17 \
  -Wall -Wextra -Werror \
  -I"${WORKDIR}" \
  -I"${ROOT}/modules/mod-fury/src" \
  "${ROOT}/modules/mod-fury/src/content/defias/DefiasContent.cpp" \
  "${ROOT}/tests/golden/defias_content_policy_golden.cpp" \
  -o "${WORKDIR}/defias_content_policy_golden"

"${WORKDIR}/defias_content_policy_golden"

OVERLAY="${ROOT}/modules/mod-fury/data/sql/db-world/updates/2026_09_23_00_fury_defias_control.sql"

grep -Eq 'UPDATE.*lw_invasion' "${OVERLAY}"
grep -Eq 'SET.*allow_random_start.*=.*0' "${OVERLAY}"
grep -Eq 'WHERE.*id.*=.*1' "${OVERLAY}"

if grep -Eiq 'DELETE[[:space:]]+FROM|TRUNCATE|DROP[[:space:]]+TABLE' "${OVERLAY}"; then
  echo "[FURY][FAIL] Defias overlay contains destructive SQL" >&2
  exit 1
fi

python3 - "${OVERLAY}" <<'PY'
import pathlib
import re
import sys

sql = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
sql = sql.replace("`", "")
lines = []
for line in sql.splitlines():
    if line.lstrip().startswith("--"):
        continue
    lines.append(line)
sql = "\n".join(lines)

statements = [s.strip() for s in sql.split(";") if s.strip()]
if len(statements) != 1:
    raise SystemExit(f"[FURY][FAIL] expected exactly one executable SQL statement, got {len(statements)}")

statement = statements[0]
patterns = [
    r"(?is)^UPDATE\s+lw_invasion\s+",
    r"(?is)SET\s+allow_random_start\s*=\s*0",
    r"(?is)WHERE\s+id\s*=\s*1\s*$",
]
for pattern in patterns:
    if not re.search(pattern, statement):
        raise SystemExit(f"[FURY][FAIL] overlay failed narrow-scope pattern: {pattern}")

print("[FURY][PASS] Defias world-DB overlay is a single narrow id=1 update")
PY

if command -v mysql >/dev/null 2>&1 && [[ -n "${MYSQL_HOST:-}" ]]; then
  MYSQL_PORT="${MYSQL_PORT:-3306}"
  MYSQL_USER="${MYSQL_USER:-root}"
  MYSQL_PASSWORD="${MYSQL_PASSWORD:-}"
  TEST_DB="${MYSQL_DATABASE:-fury_t24_world}"

  mysql_args=(
    -h "${MYSQL_HOST}"
    -P "${MYSQL_PORT}"
    -u "${MYSQL_USER}"
    --protocol=TCP
  )

  if [[ -n "${MYSQL_PASSWORD}" ]]; then
    mysql_args+=("-p${MYSQL_PASSWORD}")
  fi

  mysql "${mysql_args[@]}" -e "DROP DATABASE IF EXISTS \`${TEST_DB}\`; CREATE DATABASE \`${TEST_DB}\`;"
  mysql "${mysql_args[@]}" "${TEST_DB}" <<'SQL'
CREATE TABLE lw_invasion (
  id INT UNSIGNED NOT NULL PRIMARY KEY,
  allow_random_start TINYINT UNSIGNED NOT NULL DEFAULT 1
);
INSERT INTO lw_invasion (id, allow_random_start) VALUES
  (1, 1),
  (2, 1);
SQL

  mysql "${mysql_args[@]}" "${TEST_DB}" < "${OVERLAY}"
  mysql "${mysql_args[@]}" "${TEST_DB}" < "${OVERLAY}"

  defias_random="$(mysql "${mysql_args[@]}" -N -s "${TEST_DB}" -e "SELECT allow_random_start FROM lw_invasion WHERE id=1;")"
  other_random="$(mysql "${mysql_args[@]}" -N -s "${TEST_DB}" -e "SELECT allow_random_start FROM lw_invasion WHERE id=2;")"

  [[ "${defias_random}" == "0" ]]
  [[ "${other_random}" == "1" ]]

  mysql "${mysql_args[@]}" -e "DROP DATABASE \`${TEST_DB}\`;"

  echo "[FURY][PASS] Defias overlay is idempotent and leaves non-Defias invasions unchanged"
fi

echo "[FURY][PASS] T24 Defias content/overlay gate passed"
