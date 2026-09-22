# FURY Azeroth — Implementation Status

Date: 2026-09-22

## Current state

- Product/content architecture: defined through v0.2/v0.3 design passes.
- Implementation contract: GREEN as repository policy/documentation.
- M1–M3 executable backlog: READY.
- GitHub repository access: GREEN.
- T00.1 repository intake: GREEN.
- T00.2 pinned upstream API verification: GREEN.
- T00.3 reproducible lock/resolver: GREEN; multiple clean CI runners resolved the exact pinned upstream revisions successfully.
- T01 mod-fury skeleton: IN_PROGRESS pending compile/start acceptance.
- T02 module-owned database: IN_PROGRESS on `agent/t02-fury-db`; source/schema present, compile/start acceptance pending.
- T03 FuryApp composition root: IN_PROGRESS on `agent/t03-fury-app`; throttled lifecycle implemented, compile/regression proof pending.
- T04 actor classification: IN_PROGRESS on `agent/t04-actor-resolver`; Human/HouseholdAltBot/RandomPlayerBot/System resolution implemented, compile/tests pending.
- T05 Household: IN_PROGRESS on `agent/t05-household`; schema/repository/service and alt-bot lookup wired into the composition root, compile/tests pending.
- T06 durable Event Store: IN_PROGRESS on `agent/t06-event-store`; schema, SHA-256 dedupe and append/query path implemented, compile/DB tests pending.
- T07 consumer replay: IN_PROGRESS on `agent/t07-event-replay`; checkpoints, ordered replay and at-least-once bus implemented, compile/replay tests pending.
- T08 AzerothCore event collector: IN_PROGRESS on `agent/t08-event-collector`; login/level/zone/quest/kill/loot/craft normalization is implemented with random population bot filtering, compile/hook tests pending.
- T09 Reward Registry: IN_PROGRESS on `agent/t09-reward-claims`; power-band policy and idempotent claim uniqueness implemented, delivery intentionally deferred, compile/DB tests pending.
- T10 Chronicle: IN_PROGRESS on `agent/t10-chronicle`; sparse household history projection is registered as a durable replay consumer, compile/replay tests pending.
- T11 diagnostics: IN_PROGRESS on `agent/t11-diagnostics`; `.fury status/actor/household/event/reward/validate` implemented, compile/permission/runtime tests pending.
- T12 M1 gate: IN_PROGRESS on `agent/t12-m1-gate`; fast MySQL 8 schema/idempotency gate is GREEN, full locked build/worldserver acceptance still required.
- M1 overall: IN_PROGRESS.
- M2/M3: READY, blocked by prior milestone gates.

## Repository baseline

Canonical repository:

`FuryArchive/FURY-Azeroth`

Default branch:

`main`

Bootstrap commit inspected:

`8fd91a9a6a9af3de2ad441117a8ee29e8f295a0c`

The repository initially contained only bootstrap documentation. No AzerothCore checkout, external modules, first-party implementation, CI, or lockfile existed, so there was no pre-existing source to overwrite.

## Gate 0 work completed

Created and verified:

- `vendor/lock/fury.lock.yaml` with immutable commits for the Playerbots AzerothCore fork, mod-playerbots, Individual Progression, and Living World;
- `docs/REPOSITORY_INTAKE.md`;
- `docs/UPSTREAM_API_NOTES.md`;
- `scripts/sync-upstreams.sh`;
- ignored generated `upstream/` workspace;
- reproducible build wrapper `scripts/build.sh`;
- GitHub Actions workflow `.github/workflows/ci.yml`.

Exact pins are source-verified but the combined build is not yet declared compatible until CI proves it.

## Verified pinned APIs

Against the exact locked revisions:

- Playerbots exports `IsRealPlayer(Player*)`;
- AzerothCore exposes the required `DatabaseScript` module-database lifecycle;
- PlayerScript exposes the M1/M2 event-collector hooks;
- WorldScript provides startup/update/shutdown lifecycle;
- Living World already publicly exposes controlled invasion start, runtime lookup, runtime signals, and authored-data queries;
- Defias invasion/stage/spawn-group/signal IDs were captured from the locked SQL, not from the design document;
- Living World objective/manual stage completion remains reserved/unimplemented, so FURY owns objectives and uses deterministic runtime signals for stage transitions.

See `docs/UPSTREAM_API_NOTES.md`.

## T01 implementation present

A first `modules/mod-fury` shell now exists with:

- module loader;
- `FuryWorldScript`;
- `Fury::App` lifecycle shell;
- `mod_fury.conf.dist`;
- module README.

This is intentionally still `IN_PROGRESS`, not `GREEN`, until the locked workspace compiles and startup acceptance is exercised.

## Immediate next gate

1. Trigger CI against the locked workspace.
2. Inspect the first real configure/build failure.
3. Fix only verified failures.
4. When compile is green, add the startup/database smoke required for M1 rather than weakening the acceptance criteria.
5. Validate T02 compile and module-owned `acore_fury` create/populate/update/restart behavior once T01 compile proof completes.
