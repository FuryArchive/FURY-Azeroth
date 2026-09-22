#!/usr/bin/env python3
import concurrent.futures
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys


def is_fury_source(path: str) -> bool:
    return "mod-fury" in Path(path).parts


def command_args(entry: dict) -> list[str]:
    if "arguments" in entry:
        return list(entry["arguments"])
    return shlex.split(entry["command"])


def output_path(args: list[str], cwd: Path) -> Path | None:
    for index, value in enumerate(args[:-1]):
        if value == "-o":
            path = Path(args[index + 1])
            return path if path.is_absolute() else cwd / path
    return None


def compile_entry(entry: dict) -> tuple[str, int, str]:
    cwd = Path(entry["directory"])
    args = command_args(entry)
    output = output_path(args, cwd)
    if output is not None:
        output.parent.mkdir(parents=True, exist_ok=True)

    source = entry["file"]
    proc = subprocess.run(
        args,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    return source, proc.returncode, proc.stdout


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: compile-fury-only.py <compile_commands.json>", file=sys.stderr)
        return 2

    database_path = Path(sys.argv[1])
    entries = json.loads(database_path.read_text())
    fury_entries = [entry for entry in entries if is_fury_source(entry["file"])]

    if not fury_entries:
        print("[FURY][FAIL] compile database contains no mod-fury sources", file=sys.stderr)
        return 1

    jobs = max(1, int(os.environ.get("FURY_BUILD_JOBS", "4")))
    print(f"[FURY] compiling {len(fury_entries)} mod-fury translation unit(s) with {jobs} worker(s)")

    failures: list[tuple[str, str]] = []

    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [pool.submit(compile_entry, entry) for entry in fury_entries]
        for future in concurrent.futures.as_completed(futures):
            source, returncode, output = future.result()
            short = Path(source).name
            if returncode == 0:
                print(f"[FURY][PASS] {short}")
            else:
                print(f"[FURY][FAIL] {source}", file=sys.stderr)
                failures.append((source, output))

    if failures:
        for source, output in failures:
            print(f"\n===== {source} =====\n{output}", file=sys.stderr)
        return 1

    print("[FURY] mod-fury compile gate passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
