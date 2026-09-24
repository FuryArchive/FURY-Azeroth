#!/usr/bin/env python3
"""Merge keyed CSV row deltas into WoW 3.3.5a WDBC files.

FURY uses this to layer Delves/Mythic+ rows on top of the matched
Worgen/Goblin HD DBC baseline (or extracted client/server DBCs).
Only fixed-width 4-byte-field WDBC tables are supported intentionally.
"""
from __future__ import annotations

import argparse
import csv
import struct
from pathlib import Path
from typing import Iterable

WDBC_HEADER = struct.Struct("<4s4I")

STRING_FIELDS: dict[str, set[str]] = {
    "Map": {"Directory"},
    "LoadingScreens": {"Name", "FileName"},
    "CreatureDisplayInfo": {
        "TextureVariation_1", "TextureVariation_2", "TextureVariation_3",
        "PortraitTextureName",
    },
    "CreatureModelData": {"ModelName"},
    "SoundEntries": {
        "Name", "DirectoryBase",
        *(f"File_{i}" for i in range(1, 11)),
    },
    "ZoneMusic": {"SetName"},
    "MapDifficulty": {"Difficultystring"},
    "ItemDisplayInfo": {
        "ModelName_1", "ModelName_2", "ModelTexture_1", "ModelTexture_2",
        "InventoryIcon_1", "InventoryIcon_2",
        *(f"Texture_{i}" for i in range(1, 9)),
    },
}

FLOAT_FIELDS: dict[str, set[str]] = {
    "Map": {"MinimapIconScale", "CorpseX", "CorpseY"},
    "AreaTable": {"MinElevation", "Ambient_Multiplier"},
    "CreatureDisplayInfo": {"CreatureModelScale"},
    "CreatureModelData": {
        "ModelScale", "FootprintTextureLength", "FootprintTextureWidth",
        "FootprintParticleScale", "CollisionWidth", "CollisionHeight",
        "MountHeight", "GeoBoxMinX", "GeoBoxMinY", "GeoBoxMinZ",
        "GeoBoxMaxX", "GeoBoxMaxY", "GeoBoxMaxZ", "WorldEffectScale",
        "AttachedEffectScale", "MissileCollisionRadius",
        "MissileCollisionPush", "MissileCollisionRaise",
    },
    "SoundEntries": {"Volumefloat", "MinDistance", "DistanceCutoff"},
    "WorldSafelocs": {"LocX", "LocY", "LocZ"},
}

TABLE_ALIASES = {
    "creaturedisplayinfo": "CreatureDisplayInfo",
    "creaturemodeldata": "CreatureModelData",
    "itemdisplayinfo": "ItemDisplayInfo",
    "areatable": "AreaTable",
    "loadingscreens": "LoadingScreens",
    "mapdifficulty": "MapDifficulty",
    "soundentries": "SoundEntries",
    "worldsafelocs": "WorldSafelocs",
    "zonemusic": "ZoneMusic",
    "item": "Item",
    "map": "Map",
}


def canonical_table(name: str) -> str:
    key = Path(name).stem.replace("_", "").lower()
    try:
        return TABLE_ALIASES[key]
    except KeyError as exc:
        raise SystemExit(f"unsupported DBC table: {name}") from exc


def is_string_field(table: str, field: str) -> bool:
    if "_Lang_" in field and not field.endswith("_Mask"):
        return True
    return field in STRING_FIELDS.get(table, set())


def is_float_field(table: str, field: str) -> bool:
    return field in FLOAT_FIELDS.get(table, set())


def parse_existing_strings(block: bytes) -> dict[bytes, int]:
    offsets: dict[bytes, int] = {b"": 0}
    pos = 0
    while pos < len(block):
        end = block.find(b"\0", pos)
        if end < 0:
            break
        value = block[pos:end]
        offsets.setdefault(value, pos)
        pos = end + 1
    return offsets


def encode_string(value: str, block: bytearray, offsets: dict[bytes, int]) -> int:
    if value == "":
        return 0
    raw = value.encode("utf-8")
    existing = offsets.get(raw)
    if existing is not None:
        return existing
    offset = len(block)
    block.extend(raw)
    block.append(0)
    offsets[raw] = offset
    return offset


def pack_field(
    table: str,
    field: str,
    value: str,
    string_block: bytearray,
    string_offsets: dict[bytes, int],
) -> bytes:
    value = value.strip()
    if is_string_field(table, field):
        return struct.pack("<I", encode_string(value, string_block, string_offsets))
    if is_float_field(table, field):
        number = float(value) if value else 0.0
        return struct.pack("<f", number)
    number = int(value, 0) if value else 0
    return struct.pack("<I", number & 0xFFFFFFFF)


def load_delta(csv_path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with csv_path.open("r", encoding="utf-8-sig", newline="") as fh:
        reader = csv.DictReader(fh)
        if not reader.fieldnames:
            raise SystemExit(f"{csv_path}: missing CSV header")
        rows = list(reader)
        return list(reader.fieldnames), rows


def merge(base_path: Path, csv_paths: Iterable[Path], out_path: Path, table: str | None) -> None:
    blob = base_path.read_bytes()
    if len(blob) < WDBC_HEADER.size:
        raise SystemExit(f"{base_path}: too small for WDBC")

    magic, record_count, field_count, record_size, string_size = WDBC_HEADER.unpack_from(blob)
    if magic != b"WDBC":
        raise SystemExit(f"{base_path}: expected WDBC, got {magic!r}")
    if record_size != field_count * 4:
        raise SystemExit(
            f"{base_path}: unsupported record layout: record_size={record_size}, "
            f"field_count={field_count}"
        )

    records_start = WDBC_HEADER.size
    records_end = records_start + record_count * record_size
    strings_end = records_end + string_size
    if strings_end > len(blob):
        raise SystemExit(f"{base_path}: truncated WDBC payload")

    records = [
        bytearray(blob[pos:pos + record_size])
        for pos in range(records_start, records_end, record_size)
    ]
    index: dict[int, int] = {
        struct.unpack_from("<I", rec, 0)[0]: idx for idx, rec in enumerate(records)
    }

    string_block = bytearray(blob[records_end:strings_end])
    if not string_block:
        string_block = bytearray(b"\0")
    elif string_block[0] != 0:
        raise SystemExit(f"{base_path}: invalid WDBC string block (offset 0 is not NUL)")
    string_offsets = parse_existing_strings(bytes(string_block))

    resolved_table = canonical_table(table or base_path.name)
    total_changed = 0

    for csv_path in csv_paths:
        csv_table = canonical_table(csv_path.name)
        if csv_table != resolved_table:
            raise SystemExit(
                f"{csv_path}: table {csv_table} does not match base table {resolved_table}"
            )
        fields, delta_rows = load_delta(csv_path)
        if len(fields) != field_count:
            raise SystemExit(
                f"{csv_path}: {len(fields)} columns but {base_path} has {field_count} fields"
            )
        if not fields or fields[0] != "ID":
            raise SystemExit(f"{csv_path}: first field must be ID")

        for row in delta_rows:
            packed = bytearray()
            for field in fields:
                packed.extend(
                    pack_field(
                        resolved_table,
                        field,
                        row.get(field, ""),
                        string_block,
                        string_offsets,
                    )
                )
            if len(packed) != record_size:
                raise AssertionError("internal record packing size mismatch")
            row_id = struct.unpack_from("<I", packed, 0)[0]
            if row_id in index:
                records[index[row_id]] = packed
            else:
                index[row_id] = len(records)
                records.append(packed)
            total_changed += 1

    header = WDBC_HEADER.pack(
        b"WDBC",
        len(records),
        field_count,
        record_size,
        len(string_block),
    )
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("wb") as fh:
        fh.write(header)
        for rec in records:
            fh.write(rec)
        fh.write(string_block)

    print(
        f"[FURY][DBC][PASS] {resolved_table}: "
        f"{record_count} -> {len(records)} records; "
        f"{total_changed} delta row(s); {out_path}"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", required=True, type=Path)
    parser.add_argument("--delta", required=True, action="append", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--table")
    args = parser.parse_args()
    merge(args.base, args.delta, args.output, args.table)


if __name__ == "__main__":
    main()
