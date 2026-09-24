#!/usr/bin/env python3
"""Merge Delves CSV rows into existing WoW 3.3.5a WDBC files.

The Delves repository ships additive CSV rows, not complete DBC tables. This
utility preserves every base DBC record/string and replaces or appends custom
rows by the first-column record ID.

For CI/schema validation only, --allow-empty-base can create a custom-only DBC.
Production packaging must always provide the real base 3.3.5a DBC directory.
"""

from __future__ import annotations

import argparse
import csv
import struct
from pathlib import Path

SCHEMAS = {
    "Map.csv": {
        "strings": {"Directory"},
        "floats": {"MinimapIconScale", "CorpseX", "CorpseY"},
    },
    "MapDifficulty.csv": {
        "strings": {"Difficultystring"},
        "floats": set(),
    },
    "AreaTable.csv": {
        "strings": set(),
        "floats": {"MinElevation", "Ambient_Multiplier"},
    },
    "LoadingScreens.csv": {
        "strings": {"Name", "FileName"},
        "floats": set(),
    },
    "WorldSafelocs.csv": {
        "strings": set(),
        "floats": {"LocX", "LocY", "LocZ"},
    },
    "CreatureDisplayInfo.csv": {
        "strings": {"TextureVariation_1", "TextureVariation_2", "TextureVariation_3", "PortraitTextureName"},
        "floats": {"CreatureModelScale"},
    },
    "CreatureModelData.csv": {
        "strings": {"ModelName"},
        "floats": {
            "ModelScale", "FootprintTextureLength", "FootprintTextureWidth",
            "FootprintParticleScale", "CollisionWidth", "CollisionHeight",
            "MountHeight", "GeoBoxMinX", "GeoBoxMinY", "GeoBoxMinZ",
            "GeoBoxMaxX", "GeoBoxMaxY", "GeoBoxMaxZ", "WorldEffectScale",
            "AttachedEffectScale", "MissileCollisionRadius",
            "MissileCollisionPush", "MissileCollisionRaise",
        },
    },
    "SoundEntries.csv": {
        "strings": {"Name", "DirectoryBase"} | {f"File_{i}" for i in range(1, 11)},
        "floats": {"Volumefloat", "MinDistance", "DistanceCutoff"},
    },
    "ZoneMusic.csv": {
        "strings": {"SetName"},
        "floats": set(),
    },
}

HEADER = struct.Struct("<4s4I")


def is_locale_string(field: str) -> bool:
    return "_Lang_" in field and not field.endswith("_Mask")


def parse_base(path: Path, field_count: int, allow_empty: bool):
    if not path.exists():
        if not allow_empty:
            raise SystemExit(f"missing base DBC: {path}")
        return [], b"\0"

    raw = path.read_bytes()
    if len(raw) < HEADER.size:
        raise SystemExit(f"invalid DBC header: {path}")
    magic, count, base_fields, record_size, string_size = HEADER.unpack_from(raw)
    if magic != b"WDBC":
        raise SystemExit(f"unsupported DBC magic {magic!r}: {path}")
    if base_fields != field_count:
        raise SystemExit(
            f"field-count mismatch for {path.name}: base={base_fields} csv={field_count}"
        )
    if record_size != field_count * 4:
        raise SystemExit(
            f"record-size mismatch for {path.name}: {record_size} != {field_count * 4}"
        )

    records_start = HEADER.size
    records_end = records_start + count * record_size
    strings_end = records_end + string_size
    if strings_end != len(raw):
        raise SystemExit(f"truncated/extra data in DBC: {path}")

    records = [
        raw[records_start + i * record_size : records_start + (i + 1) * record_size]
        for i in range(count)
    ]
    strings = raw[records_end:strings_end]
    if not strings:
        strings = b"\0"
    return records, strings


def string_allocator(initial: bytes):
    block = bytearray(initial)
    if not block or block[0] != 0:
        raise SystemExit("DBC string block must begin with NUL")

    offsets = {"": 0}
    pos = 1
    while pos < len(block):
        end = block.find(0, pos)
        if end < 0:
            break
        try:
            value = bytes(block[pos:end]).decode("utf-8")
        except UnicodeDecodeError:
            value = ""
        if value and value not in offsets:
            offsets[value] = pos
        pos = end + 1

    def alloc(value: str) -> int:
        if not value:
            return 0
        if value in offsets:
            return offsets[value]
        offset = len(block)
        block.extend(value.encode("utf-8"))
        block.append(0)
        offsets[value] = offset
        return offset

    return block, alloc


def pack_row(row: dict[str, str], fields: list[str], schema: dict, alloc) -> bytes:
    chunks = []
    strings = schema["strings"]
    floats = schema["floats"]

    for field in fields:
        raw = (row.get(field) or "").strip()
        if field in strings or is_locale_string(field):
            chunks.append(struct.pack("<I", alloc(raw)))
        elif field in floats:
            chunks.append(struct.pack("<f", float(raw or "0")))
        else:
            value = int(float(raw or "0"))
            chunks.append(struct.pack("<I", value & 0xFFFFFFFF))
    return b"".join(chunks)


def merge_one(csv_path: Path, base_dir: Path, out_dir: Path, allow_empty: bool):
    schema = SCHEMAS[csv_path.name]
    with csv_path.open("r", encoding="utf-8-sig", newline="") as fh:
        reader = csv.DictReader(fh)
        fields = reader.fieldnames or []
        rows = list(reader)

    if not fields or fields[0] != "ID":
        raise SystemExit(f"{csv_path.name}: first field must be ID")

    dbc_name = csv_path.with_suffix(".dbc").name
    base_records, base_strings = parse_base(base_dir / dbc_name, len(fields), allow_empty)
    string_block, alloc = string_allocator(base_strings)

    order: list[int] = []
    records_by_id: dict[int, bytes] = {}
    for record in base_records:
        record_id = struct.unpack_from("<I", record, 0)[0]
        if record_id not in records_by_id:
            order.append(record_id)
        records_by_id[record_id] = record

    custom_ids = []
    for row in rows:
        record_id = int(row["ID"])
        packed = pack_row(row, fields, schema, alloc)
        if len(packed) != len(fields) * 4:
            raise AssertionError("packed record has wrong size")
        if record_id not in records_by_id:
            order.append(record_id)
        records_by_id[record_id] = packed
        custom_ids.append(record_id)

    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / dbc_name
    records = [records_by_id[i] for i in order]
    header = HEADER.pack(b"WDBC", len(records), len(fields), len(fields) * 4, len(string_block))
    out_path.write_bytes(header + b"".join(records) + bytes(string_block))

    print(
        f"[FURY][DBC] {dbc_name}: base={len(base_records)} custom={len(custom_ids)} "
        f"final={len(records)} fields={len(fields)}"
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv-dir", type=Path, required=True)
    parser.add_argument("--base-dir", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--allow-empty-base", action="store_true")
    args = parser.parse_args()

    missing = [name for name in SCHEMAS if not (args.csv_dir / name).exists()]
    if missing:
        raise SystemExit("missing Delves CSV files: " + ", ".join(missing))

    for name in SCHEMAS:
        merge_one(args.csv_dir / name, args.base_dir, args.out_dir, args.allow_empty_base)


if __name__ == "__main__":
    main()
