# FURY Azeroth — Delves Integration

Date: 2026-09-24

The pinned Araxia Delves repository is a multi-part content bundle rather than an AzerothCore C++ module. FURY integrates it as server content + standard-Eluna scripts + server navigation data + client merge inputs.

## Runtime dependency

Delves contains 28 Lua scripts for boss mechanics and the teleporter. The FURY gate validates their Lua 5.1 syntax and checks every object method / Register* or Create* global referenced by those scripts against the pinned **standard Eluna** source.

The current teleporter calls such as `IsFlying`, `InBattleground`, `InArena` and player channel event 22 exist in standard Eluna, so FURY does not need a second ALE runtime just for Delves.

## Staged server payload

`scripts/stage-delves-integration.sh` emits:

- all boss/teleporter Lua scripts;
- 53 map tiles plus matching mmap/vmap navigation/collision data for map IDs 805 and 900–911;
- an upstream SQL provenance aggregate;
- a FURY-safe world SQL aggregate;
- the nine additive DBC CSV tables.

The raw SQL is never the recommended import. Use `server/sql/fury-world.sql`.

## Reward safety

The upstream reward SQL references Araxia custom IDs `910001`, `911000` and `911001`, but the Delves repository does not define corresponding `item_template` rows. It also labels item 43949 as a "Delve Token" without defining a Delves-specific item.

FURY therefore keeps the maps, mobs, bosses, mechanics and ordinary creature loot, but the final SQL footer removes:

- reference loot table 100500;
- reward table 110000;
- boss rows that reference table 100500 or the bogus token IDs;
- Delve Chest spawns (entry 130000).

The chest template is retained for a future FURY Delves reward adapter. Until that adapter exists, players will not see empty/broken reward chests.

## DBC handling

The upstream `DBC_CSV/DBFilesClient/*.csv` files contain **additive custom rows only**. Exporting them as standalone DBC files and overwriting the base client/server DBCs would delete Blizzard's existing rows.

`scripts/merge-delves-dbc.py` instead reads a real base 3.3.5a WDBC file, preserves all existing records/string data, then replaces or appends Delves rows by record ID.

Production usage:

```bash
python3 scripts/merge-delves-dbc.py \
  --csv-dir build/delves-integration/dbc/custom-csv \
  --base-dir /path/to/clean-3.3.5a/dbc \
  --out-dir build/delves-integration/dbc/merged
```

`--allow-empty-base` exists only for CI/schema validation and must not be used for a playable client/server package.

The nine tables are:

- AreaTable
- CreatureDisplayInfo
- CreatureModelData
- LoadingScreens
- Map
- MapDifficulty
- SoundEntries
- WorldSafelocs
- ZoneMusic

They do not overlap the Worgen/Goblin custom-race tables currently staged by FURY (`ChrRaces`, `CharBaseInfo`, `Spell`), so those two integrations can be merged independently before the final Reforged client pack.

## Client packaging

The client output is merge-input only:

- the upstream MPQ directory tree;
- DelvesTeleporter addon;
- additive DBC CSV inputs.

Nothing is copied directly over Project Reforged. The eventual FURY client assembler must merge all DBC/assets and emit one coherent patch set.

## Remaining runtime gate

After standard Eluna is compile-GREEN, Delves still needs one real runtime acceptance pass:

1. merge the nine DBC tables into the actual server/client base;
2. install staged maps/mmaps/vmaps and `fury-world.sql`;
3. install the Lua scripts;
4. package the client assets with Reforged;
5. enter each map family, verify collision/pathing, fight at least one scripted boss and test DelvesTeleporter enter/leave.

That is a finite packaging/runtime task; the source architecture is now defined.
