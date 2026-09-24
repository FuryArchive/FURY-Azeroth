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
ADAPTER="${ROOT}/scripts/adapt-living-world-sql.py"
FIXTURE="${WORKDIR}/900_defias_westfall_invasion.sql"

python3 - "${OVERLAY}" "${ADAPTER}" "${FIXTURE}" <<'PY'
import ast
import pathlib
import re
import sys

overlay = pathlib.Path(sys.argv[1])
adapter = pathlib.Path(sys.argv[2])
fixture = pathlib.Path(sys.argv[3])

overlay_text = overlay.read_text(encoding="utf-8")
executable_lines = [
    line for line in overlay_text.splitlines()
    if line.strip() and not line.lstrip().startswith("--")
]
if executable_lines:
    raise SystemExit(
        "[FURY][FAIL] Defias FURY-owned overlay must remain executable-SQL free; "
        "Living World owns the source row"
    )

tree = ast.parse(adapter.read_text(encoding="utf-8"), filename=str(adapter))
values = {}
for node in tree.body:
    if not isinstance(node, ast.Assign) or len(node.targets) != 1:
        continue
    target = node.targets[0]
    if isinstance(target, ast.Name) and target.id in {"OLD", "NEW"}:
        values[target.id] = ast.literal_eval(node.value)

if set(values) != {"OLD", "NEW"}:
    raise SystemExit("[FURY][FAIL] Living World SQL adapter OLD/NEW contract missing")

old = values["OLD"]
new = values["NEW"]
if old == new:
    raise SystemExit("[FURY][FAIL] Living World SQL adapter does not change the pinned row")

old_match = re.search(r"VALUES\s*\((.*)\);", old)
new_match = re.search(r"VALUES\s*\((.*)\);", new)
if not old_match or not new_match:
    raise SystemExit("[FURY][FAIL] Living World SQL adapter row shape changed unexpectedly")

def split_row(payload: str):
    parts = []
    cur = []
    quote = False
    for ch in payload:
        if ch == "'":
            quote = not quote
            cur.append(ch)
        elif ch == "," and not quote:
            parts.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    parts.append("".join(cur).strip())
    return parts

old_fields = split_row(old_match.group(1))
new_fields = split_row(new_match.group(1))
if len(old_fields) != len(new_fields):
    raise SystemExit("[FURY][FAIL] Living World SQL adapter changes row width")

diffs = [i for i, (a, b) in enumerate(zip(old_fields, new_fields)) if a != b]
if len(diffs) != 1 or old_fields[diffs[0]] != "1" or new_fields[diffs[0]] != "0":
    raise SystemExit(
        "[FURY][FAIL] Living World SQL adapter must change exactly one boolean field 1 -> 0"
    )

fixture.write_text(old + "\n", encoding="utf-8")
print("[FURY][PASS] Defias overlay is a tracked no-op and adapter contract is one-field 1 -> 0")
PY

python3 "${ADAPTER}" "${FIXTURE}"
grep -Fq "3600, 0, 1, 'Defias attack/control Sentinel Hill'" "${FIXTURE}"
python3 "${ADAPTER}" "${FIXTURE}"
grep -Fq "3600, 0, 1, 'Defias attack/control Sentinel Hill'" "${FIXTURE}"

echo "[FURY][PASS] Defias Living World SQL adaptation is deterministic and idempotent"
echo "[FURY][PASS] T24 Defias content/overlay gate passed"
