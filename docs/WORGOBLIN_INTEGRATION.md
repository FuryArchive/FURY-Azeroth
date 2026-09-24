# FURY Azeroth — Worgen/Goblin Compatibility

Date: 2026-09-24

FURY uses the pinned `araxiaonline/mod-worgoblin` content as the source for playable Goblin and Worgen support, but does **not** apply its legacy core patch verbatim.

## Why the upstream core patch is not used directly

The pinned Playerbots AzerothCore has moved beyond the assumptions in the original patch:

- playable race masks and max race are derived dynamically from `ChrRaces.dbc` by `RaceMgr`; the old static `MAX_RACES/RACEMASK_*` edits are obsolete;
- `Player::GetReputationPriceDiscount` now exposes a ScriptMgr hook; the old Goblin early-return would bypass that hook;
- enum reflection lives in `enuminfo_SharedDefines.cpp` and must know about the new playable race constants.

FURY therefore carries a narrow Playerbots-adapted patch.

## Core changes

The FURY patch adds:

- `RACE_GOBLIN = 9`;
- `RACE_WORGEN = 12`;
- matching `EnumUtils<Races>` entries;
- faction-change language safety for Goblin/Worgen;
- Goblin `Best Deals Anywhere` handling while preserving the current ScriptMgr reputation-discount hook.

Race masks remain owned by `RaceMgr` and are populated from the custom `ChrRaces.dbc`.

## Server module

A targeted all-profile sync:

```bash
FURY_STACK_PROFILE=all \
FURY_INTEGRATION_FILTER=worgoblin \
bash scripts/sync-upstreams.sh
```

does three things:

1. resolves the pinned Worgen/Goblin integration;
2. applies the FURY core patch to the pinned Playerbots core;
3. links the integration as `modules/mod-worgoblin`.

The module can then be fast-compiled with `FURY_BUILD_TARGET=module-only`.

## Client and DBC assets

Worgen/Goblin is not server-only. The pinned integration contains:

- custom `DBFilesClient` DBCs including `ChrRaces.dbc`, `CharBaseInfo.dbc` and `Spell.dbc`;
- character models/textures;
- interface assets;
- sounds/spells/world assets;
- an HD reference overlay.

These assets **must not** be copied over the Project Reforged client blindly. Reforged and Worgen/Goblin both touch client data, including DBCs.

`scripts/stage-worgoblin-integration.sh` therefore emits merge inputs only:

```text
build/worgoblin-integration/
  server/dbc/
  client/base-patch/
  client/hd-reference/
  FURY-WORGOBLIN-MANIFEST.txt
```

The later FURY client assembler will merge these inputs with the actual Project Reforged patch set and emit one coherent client pack.

## Acceptance

The compatibility gate verifies:

- the adapted core patch applies;
- Goblin/Worgen enums and reflection exist;
- Goblin vendor discount preserves the current ScriptMgr hook;
- critical DBC and client asset trees exist;
- merge-input staging succeeds;
- `mod-worgoblin` compiles against the pinned Playerbots core.

Runtime-GREEN additionally requires a real Reforged client merge and in-game character creation/login for both races. That is intentionally separate from this source compatibility gate.
