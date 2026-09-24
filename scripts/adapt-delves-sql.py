#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

INSERT_RE = re.compile(
    r"(INSERT\s+INTO\s+`?creature`?\s*\()(.*?)(\)\s*VALUES\s*)",
    re.IGNORECASE | re.DOTALL,
)

ZEROISH = {"0", "NULL", "null"}


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


def strip_comments(text: str) -> str:
    out: list[str] = []
    i = 0
    quote: str | None = None
    escaped = False

    while i < len(text):
        ch = text[i]

        if quote is not None:
            out.append(ch)
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
            out.append(ch)
            i += 1
            continue

        if text.startswith("--", i):
            while i < len(text) and text[i] != "\n":
                i += 1
            if i < len(text):
                out.append("\n")
                i += 1
            continue

        if text.startswith("/*", i):
            end = text.find("*/", i + 2)
            if end < 0:
                raise ValueError("unterminated block comment")
            out.append(" ")
            i = end + 2
            continue

        out.append(ch)
        i += 1

    return "".join(out)


def find_statement_end(text: str, start: int) -> int:
    quote: str | None = None
    escaped = False
    line_comment = False
    block_comment = False
    depth = 0
    i = start

    while i < len(text):
        ch = text[i]

        if line_comment:
            if ch == "\n":
                line_comment = False
            i += 1
            continue

        if block_comment:
            if text.startswith("*/", i):
                block_comment = False
                i += 2
            else:
                i += 1
            continue

        if quote is not None:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                quote = None
            i += 1
            continue

        if text.startswith("--", i):
            line_comment = True
            i += 2
            continue

        if text.startswith("/*", i):
            block_comment = True
            i += 2
            continue

        if ch in ("'", '"'):
            quote = ch
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == ";" and depth == 0:
            return i
        i += 1

    raise ValueError("unterminated creature INSERT")


def parse_tuples(values: str) -> list[list[str]]:
    values = strip_comments(values)
    rows: list[list[str]] = []
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
                    rows.append(split_sql_list(values[start:i]))
                    i += 1
                    break
            i += 1
        else:
            raise ValueError("unterminated VALUES tuple")

    return rows


def normalize_id1(text: str) -> str:
    text = re.sub(r"`id1`", "`id`", text, flags=re.IGNORECASE)
    return re.sub(
        r"(?<![A-Za-z0-9_])id1(?![A-Za-z0-9_])",
        "id",
        text,
        flags=re.IGNORECASE,
    )


def adapt_file(path: Path) -> tuple[int, int]:
    original = path.read_text(encoding="utf-8")
    payload = normalize_id1(original)

    out: list[str] = []
    cursor = 0
    statements = 0
    rows_changed = 0

    while True:
        match = INSERT_RE.search(payload, cursor)
        if not match:
            out.append(payload[cursor:])
            break

        out.append(payload[cursor:match.start()])
        end = find_statement_end(payload, match.end())

        columns = split_sql_list(match.group(2))
        normalized = [c.strip().strip("`").lower() for c in columns]

        if "id2" not in normalized and "id3" not in normalized:
            out.append(payload[match.start():end + 1])
            cursor = end + 1
            continue

        if "id" not in normalized:
            raise ValueError(f"{path}: creature INSERT missing current id/id1 column")

        drop_indexes = [i for i, name in enumerate(normalized) if name in {"id2", "id3"}]
        rows = parse_tuples(payload[match.end():end])

        for row_no, values in enumerate(rows, start=1):
            if len(values) != len(columns):
                raise ValueError(
                    f"{path}: creature row {row_no}: {len(values)} values for "
                    f"{len(columns)} columns"
                )
            for idx in drop_indexes:
                value = values[idx].strip()
                if value not in ZEROISH:
                    raise ValueError(
                        f"{path}: creature row {row_no}: refusing to drop "
                        f"{normalized[idx]}={value}; alternative spawn entry needs explicit migration"
                    )

        new_columns = [c for i, c in enumerate(columns) if i not in drop_indexes]
        rebuilt_rows = []
        for values in rows:
            new_values = [v for i, v in enumerate(values) if i not in drop_indexes]
            rebuilt_rows.append("(" + ", ".join(new_values) + ")")

        out.append(match.group(1))
        out.append(", ".join(new_columns))
        out.append(match.group(3))
        out.append(",\n".join(rebuilt_rows))
        out.append(";")

        statements += 1
        rows_changed += len(rows)
        cursor = end + 1

    adapted = "".join(out)

    if re.search(r"(?<![A-Za-z0-9_])id1(?![A-Za-z0-9_])", adapted, re.IGNORECASE):
        raise ValueError(f"{path}: obsolete creature id1 token remains")
    if re.search(
        r"INSERT\s+INTO\s+`?creature`?\s*\([^;]*(?:`?id2`?|`?id3`?)",
        adapted,
        re.IGNORECASE | re.DOTALL,
    ):
        raise ValueError(f"{path}: obsolete creature id2/id3 INSERT column remains")

    if adapted != original:
        path.write_text(adapted, encoding="utf-8")

    return statements, rows_changed


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: adapt-delves-sql.py PATH/TO/DELVE/SQL/DIR", file=sys.stderr)
        return 2

    root = Path(sys.argv[1])
    if not root.is_dir():
        raise SystemExit(f"[FURY][DELVES-SQL][FAIL] SQL directory missing: {root}")

    files_changed = 0
    statements = 0
    rows = 0

    for path in sorted(root.rglob("*.sql")):
        before = path.read_text(encoding="utf-8")
        s, r = adapt_file(path)
        after = path.read_text(encoding="utf-8")
        if before != after:
            files_changed += 1
        statements += s
        rows += r

    print(
        f"[FURY][DELVES-SQL][PASS] current creature schema applied: "
        f"files={files_changed}, inserts={statements}, rows={rows}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
