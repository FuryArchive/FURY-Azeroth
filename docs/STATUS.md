# FURY Azeroth — Implementation Status

Date: 2026-09-22

## Current state

- Product/content architecture: defined through v0.2/v0.3 design passes.
- Implementation contract: GREEN as repository policy/documentation.
- GitHub repository access: GREEN.
- Gate 0 repository/API/pinning work: GREEN.
- T01 mod-fury skeleton: GREEN.
- T02 module-owned `acore_fury` database: GREEN.
- T03 FuryApp composition root: GREEN.
- T04 actor classification: GREEN.
- T05 Household: GREEN.
- T06 durable Event Store: GREEN.
- T07 consumer checkpoint/replay: GREEN.
- T08 AzerothCore event collector: GREEN.
- T09 Reward Registry kernel: GREEN.
- T10 Chronicle projection: GREEN.
- T11 diagnostics/validation shell: GREEN.
- T12 M1 automated gate: GREEN.
- **M1 FURY Kernel: GREEN.**
- T13 Campaign schema/service: GREEN.
- M2 Campaign Platform: IN_PROGRESS — T13–T16 are GREEN; T17 Profession Orders is the only active scope.
- M3 Defias Resurgence vertical slice: READY, blocked by M2.

## Repository baseline

Canonical repository:

`FuryArchive/FURY-Azeroth`

Default branch:

`main`

Bootstrap commit inspected:

`8fd91a9a6a9af3de2ad441117a8ee29e8f295a0c`

M1 implementation was merged through PR #7 into:

`9f5a9f55d0a2345f44dfa2ae5574e5f1a54dd21a`

T13 Campaign merged through PR #14 into:

`fa4e63c7651d7ee22fc02ff5f3ada2058739e81d`

## Gate 0 work completed

Created and verified:

- `vendor/lock/fury.lock.yaml` with immutable commits for the Playerbots AzerothCore fork, mod-playerbots, Individual Progression, and Living World;
- `docs/REPOSITORY_INTAKE.md`;
- `docs/UPSTREAM_API_NOTES.md`;
- `scripts/sync-upstreams.sh`;
- reproducible `worldserver` build wrapper;
- GitHub Actions M1 regression workflow.

## Verified pinned APIs

Against the exact locked revisions:

- Playerbots exports `IsRealPlayer(Player*)`;
- AzerothCore exposes the required `DatabaseScript` module-database lifecycle;
- PlayerScript exposes the event-collector hooks used by FURY;
- WorldScript provides startup/update/shutdown lifecycle;
- Living World publicly exposes controlled invasion start, runtime lookup, runtime signals, and authored-data queries;
- Defias invasion/stage/spawn-group/signal IDs were captured from the locked SQL;
- Living World objective/manual stage completion remains outside its public API, so FURY owns objectives and uses deterministic runtime signals for stage transitions.

See `docs/UPSTREAM_API_NOTES.md`.

## M1 accepted kernel

M1 now includes:

- first-party `mod-fury` module loader and lifecycle;
- module-owned `acore_fury` MySQL database with create/populate/update/restart lifecycle;
- Human / HouseholdAltBot / RandomPlayerBot / System actor classification;
- two-account Household kernel with persistence-aware cache reconciliation;
- durable normalized Fury Event Store with SHA-256 dedupe identity;
- monotonic consumer checkpoints and at-least-once replay batches;
- AzerothCore player event normalization with random population bot filtering;
- Reward Registry kernel with power-band policy, idempotent claims and terminal reconciliation;
- sparse replay-safe Chronicle projection;
- `.fury status/actor/household/event/reward/validate` diagnostics;
- disabled-mode guards so FURY does not touch its database when `Fury.Enable=0`.

## M1 acceptance evidence

Final PR #7 CI run **#91** passed every mandatory gate:

- `Fast mod-fury compile`: GREEN.
- `M1 schema golden gate`: GREEN against MySQL 8.
- `M1 actor-policy golden gate`: GREEN.
- `Locked worldserver + M1 runtime`: GREEN.
- production pinned `worldserver` build: GREEN.
- real FURY database lifecycle smoke: GREEN.
- pinned AzerothCore runtime-data install with fixed SHA-256: GREEN.
- full `worldserver` startup smoke: GREEN twice consecutively.
- each runtime smoke reached `worldserver ready`, executed `.fury validate`, returned `HEALTHY`, and shut down cleanly.

The M1 gate therefore satisfies the repository rule that source presence alone is insufficient: compile, persistence, restart, replay, and runtime acceptance have all passed.

## T13 Campaign acceptance evidence

Final PR #14 CI run **#97**, attempt **8**, passed the required Campaign and M1 regression gates:

- `T13 Campaign policy golden gate`: GREEN.
- `T13 Campaign schema golden gate`: GREEN.
- `Fast mod-fury compile`: GREEN.
- `M1 actor-policy golden gate`: GREEN.
- `M1 schema golden gate`: GREEN.
- `Locked worldserver + M1 runtime`: GREEN.
- production pinned `worldserver` build: GREEN.
- real FURY database lifecycle smoke: GREEN.
- pinned runtime-data install: GREEN.
- full `worldserver` smoke: GREEN twice consecutively.

T13 therefore satisfies its acceptance contract: backward transitions are rejected, duplicate completion is idempotent, persistent progression authority is Human-only through the central actor policy, and household power band is derived from canonical completed campaign nodes.

## Immediate next gate

Complete **T17 Minimal profession-order subsystem** only:

1. durable household order definitions/options/instances;
2. exact profession-skill + crafted-item matching;
3. Human-only acceptance/progress through central actor policy;
4. replay-safe progress and crash-recovery completion;
5. no profession XP or richer crafting progression;
6. keep M1 + T13–T16 fast regressions mandatory.

T18+ remain READY and are intentionally outside this pass.
