#!/usr/bin/env python3
import concurrent.futures
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys


SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx"}


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
    source = Path(entry["file"])
    if not source.is_absolute():
        source = cwd / source

    if not source.exists():
        return str(source), 0, "[FURY][SKIP] generated source is not present yet"

    args = command_args(entry)
    output = output_path(args, cwd)
    if output is not None:
        output.parent.mkdir(parents=True, exist_ok=True)

    proc = subprocess.run(
        args,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    return str(source), proc.returncode, proc.stdout


def main() -> int:
    if len(sys.argv) != 4:
        print(
            "usage: warm-compile-cache.py <compile_commands.json> <shard-index> <shard-count>",
            file=sys.stderr,
        )
        return 2

    database_path = Path(sys.argv[1])
    shard_index = int(sys.argv[2])
    shard_count = int(sys.argv[3])

    if shard_count < 1 or shard_index < 0 or shard_index >= shard_count:
        print("[FURY][FAIL] invalid shard coordinates", file=sys.stderr)
        return 2

    entries = json.loads(database_path.read_text())
    sources = [
        entry
        for entry in entries
        if Path(entry["file"]).suffix.lower() in SOURCE_SUFFIXES
    ]
    sources.sort(key=lambda entry: (entry["file"], entry.get("output", "")))

    shard = [
        entry for index, entry in enumerate(sources)
        if index % shard_count == shard_index
    ]

    if not shard:
        print(f"[FURY][FAIL] compile shard {shard_index}/{shard_count} is empty", file=sys.stderr)
        return 1

    jobs = max(1, int(os.environ.get("FURY_BUILD_JOBS", "4")))
    print(
        f"[FURY] warming exact compile cache shard {shard_index + 1}/{shard_count}: "
        f"{len(shard)} of {len(sources)} translation unit(s), {jobs} worker(s)"
    )

    failures: list[tuple[str, str]] = []
    skipped = 0

    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [pool.submit(compile_entry, entry) for entry in shard]
        for future in concurrent.futures.as_completed(futures):
            source, returncode, output = future.result()
            short = Path(source).name
            if output.startswith("[FURY][SKIP]"):
                skipped += 1
                print(f"{output}: {source}")
            elif returncode == 0:
                print(f"[FURY][PASS] {short}")
            else:
                print(f"[FURY][FAIL] {source}", file=sys.stderr)
                failures.append((source, output))

    if failures:
        for source, output in failures:
            print(f"\n===== {source} =====\n{output}", file=sys.stderr)
        return 1

    print(
        f"[FURY][PASS] cache shard {shard_index + 1}/{shard_count} complete "
        f"(skipped generated sources: {skipped})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
