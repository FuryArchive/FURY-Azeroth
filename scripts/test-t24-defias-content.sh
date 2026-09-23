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

echo "[FURY][PASS] T24 Defias content/overlay gate passed"
