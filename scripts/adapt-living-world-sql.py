#!/usr/bin/env python3
from pathlib import Path
import sys

OLD = "VALUES (1, 'Defias Westfall Invasion', 0, 40, 1, 1, 10, 20, 100, 79200, 115200, 3600, 1, 1, 'Defias attack/control Sentinel Hill');"
NEW = "VALUES (1, 'Defias Westfall Invasion', 0, 40, 1, 1, 10, 20, 100, 79200, 115200, 3600, 0, 1, 'Defias attack/control Sentinel Hill');"

def main() -> int:
    if len(sys.argv) != 2:
        print("usage: adapt-living-world-sql.py PATH/TO/900_defias_westfall_invasion.sql", file=sys.stderr)
        return 2

    path = Path(sys.argv[1])
    text = path.read_text(encoding="utf-8")

    if NEW in text:
        print("[FURY][LIVING-WORLD-SQL][PASS] Defias random start already disabled")
        return 0

    count = text.count(OLD)
    if count != 1:
        raise SystemExit(
            f"[FURY][LIVING-WORLD-SQL][FAIL] expected exactly one pinned Defias invasion row, found {count}"
        )

    path.write_text(text.replace(OLD, NEW, 1), encoding="utf-8")
    print("[FURY][LIVING-WORLD-SQL][PASS] disabled autonomous Defias random start")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
