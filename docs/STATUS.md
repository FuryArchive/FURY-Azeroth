# FURY Azeroth — Implementation Status

Date: 2026-09-23

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
- T20 M2 automated gate: GREEN.
- **M2 Campaign Platform: GREEN.**
- T21 Living World source audit: GREEN.
- T22 Living World external bridge patch: GREEN.
- T23 FURY LivingWorldAdapter: GREEN.
- T24 Defias content validation/random-start overlay: GREEN.
- T25 Defias Director graph: GREEN.
- T26 Westfall contract board: GREEN.
- T27 Six Defias contracts: GREEN.
- T28 Human participation resolver: GREEN.
- T29 Participation score/outcome policy: IN_PROGRESS.
- M3 Defias Resurgence vertical slice: IN_PROGRESS.

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

## T20 / M2 acceptance evidence

PR #22 merged T20 through commit `028b7aa61efd01e61e5ad72b36582fcfd31439f5`.

GitHub Actions run **#118** completed GREEN. The dedicated `T20 M2 campaign platform golden gate` passed the mandatory M1 regression plus all T13–T19 policy/schema scenarios, including campaign transitions, household/character gate separation, proof idempotency, contract replay/crash recovery, Director duplicate-start/restart persistence, Profession Order filtering/replay, Bestiary filtering/replay, and Individual Progression read-only/absence behavior.

The fast `mod-fury` compile passed in the same run. Full AzerothCore/worldserver runtime remains intentionally outside ordinary PR CI and is reserved for integration-sensitive milestones.

**M2 Campaign Platform is GREEN.**

## Immediate next gate

T21 is GREEN. See `docs/T21_LIVING_WORLD_AUDIT.md`.

T22 is GREEN through PR #23 / commit `4697a9dc58c78a0a1b44187cb416c490ccf13667`. Fast CI run #122 passed the strict pinned patch-application guard, standalone patched Living World translation-unit compile with `mod-fury` removed, Fast mod-fury compile, and M2 regressions.

T23 is GREEN through PR #24 / commit `41de42d2b7410fc2b3dfe449e2b42c1238db1d88`. Fast CI run #124 passed the adapter boundary gate, patched Living World regression and Fast mod-fury compile.

T24 is GREEN through PR #25 / commit `106f157c7676d740f868a11b4a4412c80b743119`. Fast CI run #126 passed the exact Defias content contract gate, MySQL overlay idempotency/isolation check, Fast mod-fury compile, and Living World/M2 regressions.

T25 is GREEN through PR #26 / commit `d328e889c403fcd79d84f9ca7908068d8a8f2b71`. Final FURY CI run **#131** passed the Defias graph/persistence suite, replay/presence/Living World terminal-delivery regressions, Fast mod-fury compile, and existing regression gates.

T26 is GREEN through PR #27. FURY CI **#133** and Full Runtime **#5** passed, including the pinned worldserver build, real FURY database lifecycle, world-data application and worldserver smoke.

T27 is GREEN through PR #31 / commit `cd22585fe90b93b87d1960143ce4fe968acedc92`. FURY CI **#139** passed the dedicated Defias contract runtime/replay gate, Fast mod-fury compile, and the existing M1/M2/Living World/Defias regression gates.

T28 is GREEN through PR #32 / commit `f1c314e8d47fb9c65826760e3301fab14a4fc5a4`. FURY CI **#141** passed the human participation policy gate, Fast mod-fury compile, and the full regression set.

T29 is **GREEN** through PR #33 / merge commit `cb3e4b6b1929a2949f38b110eee4e43dde075c08`. FURY CI **#159** and Full Runtime **#13** passed, including exact event-to-run score attribution, pinned worldserver build, real FURY database lifecycle, and two worldserver smokes.

T30 is **GREEN** through PR #34 / merge commit `e0f90103e60667819bcfe8362f508c5d4094cbc0`. FURY CI **#161** passed the dedicated runtime/mastery gate, Fast mod-fury compile, and the active regressions.

T31 is **GREEN** through PR #38 / merge commit `f6610fb4d31faa8f92ad26c8a8ea25964f0a9957`. FURY CI **#168** passed the dedicated adaptive Field Relief gate, Fast mod-fury compile, and all active regressions.

T32 Persistent Defias Resolution is **IN_PROGRESS** on `agent/m3-t32-persistent-resolution`: canonical Director terminal event binding, Success/Partial/Ignored projections, campaign continuation after non-success, replay-safe proofs/Chronicle, emergency pressure, and a pending-only Success reward entitlement are implemented pending branch acceptance.
