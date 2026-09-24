#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from collections import defaultdict
from pathlib import Path


HEADER_RE = re.compile(
    r"(INSERT\s+INTO\s+`(?P<table>item_template|creature_template)`\s*\()"
    r"(?P<columns>.*?)"
    r"(?P<values_prefix>\)\s*VALUES\s*)",
    re.IGNORECASE | re.DOTALL,
)

DROP_COLUMNS = {
    "item_template": {"statscount"},
    # AzerothCore 2026_03_22_03 migrated these fields out of
    # creature_template. Worgoblin's rows use zero immunity masks and already
    # provide creature_template_model.DisplayScale separately, so dropping the
    # legacy fields preserves their intended current-schema meaning.
    "creature_template": {
        "scale",
        "mechanic_immune_mask",
        "spell_school_immune_mask",
    },
}


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

    raise ValueError("unterminated Worgoblin INSERT")


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


def normalized_column(column: str) -> str:
    return column.strip().strip("`").lower()


def adapt(payload: str) -> tuple[str, dict[str, tuple[int, int]]]:
    out: list[str] = []
    cursor = 0
    stats: dict[str, list[int]] = defaultdict(lambda: [0, 0])

    while True:
        match = HEADER_RE.search(payload, cursor)
        if not match:
            out.append(payload[cursor:])
            break

        out.append(payload[cursor:match.start()])

        table = match.group("table").lower()
        columns = split_sql_list(match.group("columns"))
        normalized = [normalized_column(column) for column in columns]
        drop_names = DROP_COLUMNS[table]
        drop_indices = {
            index
            for index, name in enumerate(normalized)
            if name in drop_names
        }

        end = find_statement_end(payload, match.end())
        values_text = payload[match.end():end]

        if not drop_indices:
            out.append(payload[match.start():end + 1])
            cursor = end + 1
            continue

        tuples = parse_tuples(values_text)
        new_columns = [
            column
            for index, column in enumerate(columns)
            if index not in drop_indices
        ]

        rebuilt_rows: list[str] = []
        for row_no, values in enumerate(tuples, start=1):
            if len(values) != len(columns):
                raise ValueError(
                    f"{table} row {row_no}: {len(values)} values for "
                    f"{len(columns)} columns before adaptation"
                )

            new_values = [
                value
                for index, value in enumerate(values)
                if index not in drop_indices
            ]
            if len(new_values) != len(new_columns):
                raise AssertionError(
                    f"{table}: column/value mismatch after legacy-field removal"
                )

            rebuilt_rows.append("(" + ", ".join(new_values) + ")")

        out.append(match.group(1))
        out.append(", ".join(new_columns))
        out.append(match.group("values_prefix"))
        out.append(",\n".join(rebuilt_rows))
        out.append(";")

        stats[table][0] += 1
        stats[table][1] += len(tuples)
        cursor = end + 1

    result = "".join(out)
    return result, {
        table: (values[0], values[1])
        for table, values in stats.items()
    }


def assert_current_schema(payload: str) -> None:
    for match in HEADER_RE.finditer(payload):
        table = match.group("table").lower()
        columns = {
            normalized_column(column)
            for column in split_sql_list(match.group("columns"))
        }
        stale = columns & DROP_COLUMNS[table]
        if stale:
            names = ", ".join(sorted(stale))
            raise SystemExit(
                f"[FURY][WORGOBLIN-SQL][FAIL] "
                f"{table} still contains legacy columns: {names}"
            )


def main() -> int:
    if len(sys.argv) != 2:
        print(
            "usage: adapt-worgoblin-sql.py PATH/TO/worgoblin.sql",
            file=sys.stderr,
        )
        return 2

    path = Path(sys.argv[1])
    original = path.read_text()
    adapted, stats = adapt(original)
    assert_current_schema(adapted)

    if adapted != original:
        path.write_text(adapted)

    if not stats:
        print(
            "[FURY][WORGOBLIN-SQL][PASS] already compatible with current "
            "item_template/creature_template schema"
        )
        return 0

    details = "; ".join(
        f"{table}: {statements} INSERT(s) / {rows} row(s)"
        for table, (statements, rows) in sorted(stats.items())
    )
    print(f"[FURY][WORGOBLIN-SQL][PASS] adapted {details}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
