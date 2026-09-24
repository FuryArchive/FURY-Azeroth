# FURY Azeroth — Selected Module Stack

Date: 2026-09-24

This file is the product-level selection record for third-party modules. Exact immutable revisions live in `vendor/lock/fury.lock.yaml`.

## Profiles

`scripts/sync-upstreams.sh` supports three profiles:

- `baseline` — the already-accepted Playerbots + Individual Progression + Living World stack used by existing M1-M3 regression gates.
- `server` — baseline plus the selected server-side module stack below.
- `all` — server profile plus client/content workspaces that require a separate compatibility and packaging pass.

Examples:

```bash
bash scripts/sync-upstreams.sh
FURY_STACK_PROFILE=server bash scripts/sync-upstreams.sh
FURY_STACK_PROFILE=all bash scripts/sync-upstreams.sh
```

Existing CI remains on `baseline` until a selected module is explicitly compatibility-green. This prevents an unverified third-party module from silently invalidating already accepted FURY gates.

## Baseline — already integrated

| Module | Role |
| --- | --- |
| mod-playerbots | Bot population, household alt-bots and group support |
| mod-individual-progression | Character-level historical progression gate |
| mod-living-world | Physical invasion/event execution |
| mod-fury | FURY authority: Household, Campaign, Director, Contracts, Chronicle, Proofs, rewards |

## Selected server stack

These modules are now canonical candidates, pinned to exact commits and syncable with `FURY_STACK_PROFILE=server`.

| Module | FURY role / ownership note |
| --- | --- |
| mod-autobalance | Two-player and small-group instance scaling |
| mod-ah-bot | Private-server auction economy |
| mod-progression-system | Execution layer for staged Classic → TBC → WotLK progression; FURY remains campaign authority |
| mod-zone-difficulty | Zone/instance difficulty controls; its own Mythic mode must not become the canonical FURY Mythic+ |
| mod-rdf-expansion | Keep Classic/TBC dungeon finder access useful in later eras |
| mod-dungeon-clear | Playerbot-led dungeon routing, pulls and scripted event handling |
| mod-aoe-loot | Modern loot QoL |
| mod-account-achievements | Account-wide achievements |
| mod-war-effort | Ahn'Qiraj War Effort for the Classic campaign |
| mod-dungeon-master | Procedural/roguelike expeditions; persistent rewards must route through FURY Reward Registry |
| mod-warband-camp | Account camp/housing; future foundation for FURY Camping |
| mod-nemesis-system | Persistent emergent enemy/rival layer; FURY integration target for Chronicle/Bestiary/rewards |
| mod-challenge-modes | Optional per-character challenge rules |
| mod-quest-radar | Quest objective QoL; addon is optional but selected for the client pack |
| mod-llm-chatter | Playerbot roleplay chatter; intended local default is Ollama with an instruct-capable ~8B model |

## Selected compatibility/client stack

These are selected, pinned and part of the target product, but activation requires more than dropping a folder into AzerothCore `modules/`.

### SoloCollectionsPlatform

Use the matched `mod-solo-collections` backend + SoloCollections addon from the same platform revision.

Do not also install legacy `mod-transmog` or separate account-mount collection systems unless a future audit proves a missing feature. We want one collection/transmog authority.

### Worgen + Goblin

`araxiaonline/mod-worgoblin` is selected.

It requires:
- an AzerothCore core patch;
- custom DBC data;
- a client MPQ patch;
- reconciliation with the Playerbots core fork;
- a merged client asset strategy with Project Reforged.

It must not be enabled until that merge is verified.

### Delves

`araxiaonline/Delves` is selected as later-game side content.

It contains custom maps/assets/content and therefore belongs to the client/content compatibility pass, not the ordinary server-module build.

### MythicPlus Extended

`Ildourol/MythicPlus-extended` is the selected primary Mythic+ candidate.

Target features include:
- persistent keystone progression;
- uncapped scaling;
- affixes;
- enemy forces;
- timed/overtime completion;
- rating;
- weekly reward/vault loop.

It requires **standard Eluna semantics + AIO**. Current AzerothCore `mod-ale` explicitly states that ALE has diverged and is not compatible with standard Eluna scripts, therefore we must not assume MythicPlus Extended runs on ALE.

Pinned compatibility references:
- `ElunaLuaEngine/ElunaAzerothcore`;
- `Rochet2/AIO`.

The Eluna AzerothCore fork must be reconciled with our pinned Playerbots AzerothCore fork before activation. This is a real compatibility task, not a configuration toggle.

Other modules' Mythic-like modes must remain secondary/disabled once MythicPlus Extended becomes canonical.

## Explicitly not stacked

- `mod-solocraft`: overlaps too much with AutoBalance for our target gameplay.
- legacy `mod-transmog`: superseded by SoloCollectionsPlatform.
- separate account-mount modules: superseded by SoloCollectionsPlatform.
- `mod-guildhouse`: Warband Camp better matches the two-player/account/alt design.
- AzerothCore `mod-ale` as MythicPlus Extended runtime: not selected because its own documentation states standard Eluna scripts are incompatible.

## Compatibility order

Do not return to T31-T34 until the selected stack has been worked through in this order:

1. Sync and compile the server-only selected modules against the pinned Playerbots core.
2. Resolve C++/hook/SQL/config collisions and establish a selected-stack build gate.
3. Define authority boundaries where third-party modules can grant persistent rewards or alter progression.
4. Integrate SoloCollections and client addons.
5. Merge Worgen/Goblin client/core changes with the Project Reforged client.
6. Audit and package Delves assets/maps.
7. Solve standard-Eluna + Playerbots compatibility, then integrate MythicPlus Extended + AIO.
8. Produce one coherent FURY client/server package.
9. Resume the remaining Defias M3 acceptance tasks on top of the final stack.

## Compatibility evidence

The first selected-stack compile run reached twelve selected modules successfully before stopping at `mod-challenge-modes`:

- AutoBalance, AHBot, Progression System, Zone Difficulty, RDF Expansion;
- Dungeon Clear, AoE Loot, Account Achievements, War Effort;
- Dungeon Master, Warband Camp, Nemesis System.

`mod-challenge-modes` was written for the older `OnPlayerResurrect(..., bool)` hook. The pinned Playerbots core exposes `OnPlayerResurrect(..., bool&)`, so FURY carries a narrow compatibility patch for that signature only.

The selected-stack gate now compiles every selected module even when one fails and reports the complete failing-module set at the end. This prevents one early incompatibility from hiding later ones such as Quest Radar or LLM Chatter.

## Status

Selection: **LOCKED**

Exact revisions: **PINNED**

Baseline compatibility: **GREEN**

Selected server stack compatibility: **IN_PROGRESS**

Client/content stack compatibility: **IN_PROGRESS**
