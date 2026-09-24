#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path


HEADER_RE = re.compile(
    r"(INSERT\s+INTO\s+`item_template`\s*\()(.*?)(\)\s*VALUES\s*)",
    re.IGNORECASE | re.DOTALL,
)


def split_sql_list(text: str) -> list[str]:
    parts: list[str] = []
    start = 0
    depth = 0
    quote: str | None = None
    escaped = False

    for i, ch in enumerate(text):
        if quote is not None:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                quote = None
            continue

        if ch in ("'", '"'):
            quote = ch
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == "," and depth == 0:
            parts.append(text[start:i].strip())
            start = i + 1

    parts.append(text[start:].strip())
    return parts


def find_statement_end(text: str, start: int) -> int:
    quote: str | None = None
    escaped = False
    depth = 0

    for i in range(start, len(text)):
        ch = text[i]
        if quote is not None:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                quote = None
            continue

        if ch in ("'", '"'):
            quote = ch
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == ";" and depth == 0:
            return i

    raise ValueError("unterminated item_template INSERT")


def parse_tuples(values: str) -> list[list[str]]:
    tuples: list[list[str]] = []
    i = 0
    while i < len(values):
        while i < len(values) and (values[i].isspace() or values[i] == ","):
            i += 1
        if i >= len(values):
            break
        if values[i] != "(":
            raise ValueError(f"expected '(' in VALUES near: {values[i:i+80]!r}")

        start = i + 1
        depth = 1
        quote: str | None = None
        escaped = False
        i += 1

        while i < len(values):
            ch = values[i]
            if quote is not None:
                if escaped:
                    escaped = False
                elif ch == "\\":
                    escaped = True
                elif ch == quote:
                    quote = None
                i += 1
                continue

            if ch in ("'", '"'):
                quote = ch
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    tuples.append(split_sql_list(values[start:i]))
                    i += 1
                    break
            i += 1
        else:
            raise ValueError("unterminated VALUES tuple")

    return tuples


def adapt(payload: str) -> tuple[str, int, int]:
    out: list[str] = []
    cursor = 0
    statements = 0
    rows = 0

    while True:
        match = HEADER_RE.search(payload, cursor)
        if not match:
            out.append(payload[cursor:])
            break

        out.append(payload[cursor:match.start()])
        columns = split_sql_list(match.group(2))
        normalized = [c.strip().strip("`").lower() for c in columns]

        end = find_statement_end(payload, match.end())
        values_text = payload[match.end():end]
        tuples = parse_tuples(values_text)

        if "statscount" not in normalized:
            out.append(payload[match.start():end + 1])
            cursor = end + 1
            continue

        idx = normalized.index("statscount")
        new_columns = columns[:idx] + columns[idx + 1:]

        rebuilt_rows: list[str] = []
        for row_no, values in enumerate(tuples, start=1):
            if len(values) != len(columns):
                raise ValueError(
                    f"item_template row {row_no}: {len(values)} values for "
                    f"{len(columns)} columns before adaptation"
                )
            new_values = values[:idx] + values[idx + 1:]
            if len(new_values) != len(new_columns):
                raise AssertionError("column/value mismatch after StatsCount removal")
            rebuilt_rows.append("(" + ", ".join(new_values) + ")")

        out.append(match.group(1))
        out.append(", ".join(new_columns))
        out.append(match.group(3))
        out.append(",\n".join(rebuilt_rows))
        out.append(";")

        statements += 1
        rows += len(tuples)
        cursor = end + 1

    return "".join(out), statements, rows


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: adapt-worgoblin-sql.py PATH/TO/worgoblin.sql", file=sys.stderr)
        return 2

    path = Path(sys.argv[1])
    original = path.read_text()
    adapted, statements, rows = adapt(original)

    if statements == 0:
        if "StatsCount" in original:
            raise SystemExit("[FURY][WORGOBLIN-SQL][FAIL] StatsCount remains but no compatible INSERT was adapted")
        print("[FURY][WORGOBLIN-SQL] already compatible; no StatsCount item_template INSERTs")
        return 0

    if re.search(r"INSERT\s+INTO\s+`item_template`.*?`StatsCount`", adapted, re.I | re.S):
        raise SystemExit("[FURY][WORGOBLIN-SQL][FAIL] StatsCount remains in item_template INSERT")

    path.write_text(adapted)
    print(f"[FURY][WORGOBLIN-SQL][PASS] adapted {statements} item_template INSERT statements / {rows} rows")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
