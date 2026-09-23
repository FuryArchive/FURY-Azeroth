#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC_ROOT="${1:-${ROOT}/upstream/azerothcore-wotlk}"

SHARED="${AC_ROOT}/src/server/shared/SharedDefines.h"
ITEMS="${AC_ROOT}/data/sql/base/db_world/item_template.sql"

[[ -f "${SHARED}" ]]
[[ -f "${ITEMS}" ]]

python3 - "${SHARED}" "${ITEMS}" <<'PY'
import pathlib
import re
import sys

shared = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
items_path = pathlib.Path(sys.argv[2])

skills = {
    "SKILL_FIRST_AID": 129,
    "SKILL_BLACKSMITHING": 164,
    "SKILL_LEATHERWORKING": 165,
    "SKILL_ALCHEMY": 171,
    "SKILL_COOKING": 185,
    "SKILL_TAILORING": 197,
    "SKILL_ENGINEERING": 202,
}

for name, value in skills.items():
    if not re.search(rf"\b{re.escape(name)}\s*=\s*{value}\b", shared):
        raise SystemExit(f"[FURY][FAIL] pinned skill id mismatch: {name}={value}")
    print(f"[FURY][PASS] pinned skill id {name}={value}")

expected_items = {
    1251: "Linen Bandage",
    118: "Minor Healing Potion",
    2679: "Charred Wolf Meat",
    2304: "Light Armor Kit",
    2862: "Rough Sharpening Stone",
    2996: "Bolt of Linen Cloth",
    4357: "Rough Blasting Powder",
}

found = {item_id: False for item_id in expected_items}

with items_path.open("r", encoding="utf-8", errors="replace") as handle:
    for line in handle:
        for item_id, name in expected_items.items():
            if found[item_id]:
                continue
            if re.search(rf"\(\s*{item_id}\s*,", line) and name in line:
                found[item_id] = True
                print(f"[FURY][PASS] pinned world item {item_id} = {name}")

missing = [
    f"{item_id}:{expected_items[item_id]}"
    for item_id, ok in found.items()
    if not ok
]
if missing:
    raise SystemExit(
        "[FURY][FAIL] pinned world item rows not found: " + ", ".join(missing)
    )
PY

echo "[FURY][PASS] T31 pinned upstream profession/item audit passed"
