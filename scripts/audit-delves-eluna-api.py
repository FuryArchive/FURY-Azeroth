#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lua-dir", type=Path, required=True)
    ap.add_argument("--eluna-dir", type=Path, required=True)
    args = ap.parse_args()

    lua_files = sorted(args.lua_dir.rglob("*.lua"))
    if not lua_files:
        raise SystemExit("[FURY][DELVES][FAIL] no Lua scripts found")

    method_re = re.compile(r":\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(")
    global_re = re.compile(r"(?<![\w.:])((?:Register|Create)[A-Za-z_][A-Za-z0-9_]*)\s*\(")

    methods: set[str] = set()
    globals_: set[str] = set()
    for path in lua_files:
        text = path.read_text(encoding="utf-8", errors="replace")
        methods.update(method_re.findall(text))
        globals_.update(global_re.findall(text))

    source_parts = []
    for path in args.eluna_dir.rglob("*"):
        if path.is_file() and path.suffix in {".h", ".hpp", ".cpp"}:
            source_parts.append(path.read_text(encoding="utf-8", errors="ignore"))
    source = "\n".join(source_parts)
    if not source:
        raise SystemExit("[FURY][DELVES][FAIL] Eluna source tree is empty")

    missing_methods = [name for name in sorted(methods) if f'"{name}"' not in source]
    missing_globals = [name for name in sorted(globals_) if name not in source]

    print(f"[FURY][DELVES] Lua scripts: {len(lua_files)}")
    print(f"[FURY][DELVES] object methods referenced: {len(methods)}")
    print(f"[FURY][DELVES] register/create globals referenced: {len(globals_)}")

    if missing_methods or missing_globals:
        if missing_methods:
            print("[FURY][DELVES][FAIL] methods missing from standard Eluna registrations:")
            for name in missing_methods:
                print(f"  - {name}")
        if missing_globals:
            print("[FURY][DELVES][FAIL] globals missing from standard Eluna source:")
            for name in missing_globals:
                print(f"  - {name}")
        raise SystemExit(1)

    print("[FURY][DELVES][PASS] Lua API surface is present in standard Eluna")


if __name__ == "__main__":
    main()
