#!/usr/bin/env python3
import json
import subprocess
import sys
from pathlib import Path

LOCK = "vendor/lock/fury.lock.yaml"

RUNTIME_PATHS = {
    "modules/mod-fury/mod-fury.cmake",
    "modules/mod-fury/src/FuryModule.cpp",
    "modules/mod-fury/src/mod_fury_loader.cpp",
    "modules/mod-fury/src/database/FuryDatabaseScript.cpp",
    "scripts/smoke-fury-db-lifecycle.sh",
    "scripts/smoke-worldserver.sh",
    "scripts/fetch-ac-data.sh",
}

RUNTIME_PREFIXES = (
    "modules/mod-fury/conf/",
    "vendor/patches/mod-living-world/",
)


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], text=True).strip()


def read_lock_at(rev: str) -> dict:
    raw = git("show", f"{rev}:{LOCK}")
    return json.loads(raw)


def baseline_projection(lock: dict) -> dict:
    modules = {
        key: value
        for key, value in lock.get("modules", {}).items()
        if value.get("tier", "baseline") == "baseline"
    }
    return {"core": lock.get("core"), "modules": modules}


def needs_full_runtime(base: str, head: str) -> tuple[bool, list[str]]:
    changed = [p for p in git("diff", "--name-only", base, head).splitlines() if p]
    reasons: list[str] = []

    for path in changed:
        if path in RUNTIME_PATHS or path.startswith(RUNTIME_PREFIXES):
            reasons.append(path)

    if LOCK in changed:
        before = baseline_projection(read_lock_at(base))
        after = baseline_projection(read_lock_at(head))
        if before != after:
            reasons.append("baseline/core entries changed in vendor lock")

    return bool(reasons), reasons


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {Path(sys.argv[0]).name} BASE_SHA HEAD_SHA", file=sys.stderr)
        return 2

    run, reasons = needs_full_runtime(sys.argv[1], sys.argv[2])
    print("true" if run else "false")
    for reason in reasons:
        print(f"[FURY] full-runtime reason: {reason}", file=sys.stderr)
    if not run:
        print("[FURY] selected-only/vendor change: skipping expensive baseline full runtime", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
