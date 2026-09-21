# FURY Azeroth — Agent Contract

## Role

The active implementation agent is ChatGPT. Do not write plans for a hypothetical future agent. When the repository is available, inspect it, change it, run its tests/builds, diagnose failures, and continue until the current milestone gate is green or a concrete external blocker is proven.

## Source of truth order

1. `docs/IMPLEMENTATION_SPEC.md` / current accepted FURY architecture.
2. `TASKS_M1_M3.md` acceptance criteria and dependency order.
3. Pinned upstream source in `fury.lock.yaml` and the exact checked-out repository state.
4. Current AzerothCore / Playerbots / required module APIs at those pins.
5. Existing project tests and migrations.

Never silently replace a pinned API assumption with knowledge from another branch.

## Product invariants

- FURY Azeroth is a long-lived two-human cooperative game, not a generic public-server module pack.
- `mod-fury` is the first-party gameplay authority for household state, FURY campaign meaning, Director decisions, Chronicle, Contracts, Proofs, and custom reward policy.
- External modules execute specialized mechanics. They do not become the authority for FURY campaign state.
- Playerbots may assist gameplay but must not independently advance persistent FURY progression.
- Household progression and per-character eligibility are separate concepts.
- FURY custom power progression has one reward authority. No first-party subsystem grants persistent rewards ad hoc.
- Durable event handling is at-least-once. Every domain mutation that consumes durable events must therefore be idempotent.
- Core patches are forbidden unless a verified missing hook/API makes the feature impossible as a module. A core patch requires a short written proof of necessity.
- Living World owns physical invasion execution. FURY Director owns when/why the event starts and what its outcome means.
- For M1–M3, no custom client MPQ or mandatory addon is allowed.

## Engineering rules

### Repository safety

- Never overwrite user work blindly.
- Inspect `git status`, branch, remotes, submodules, and current HEAD before editing.
- Preserve unrelated local changes.
- Never force-push.
- Never update upstream modules to “latest” during implementation. Use the lock file.
- Any upstream change must be an explicit lockfile change with compatibility notes.

### Work discipline

For each task:

1. Read the relevant implementation spec section and current source.
2. Verify the actual upstream API before coding against it.
3. Add/adjust the smallest useful test first when practical.
4. Implement the minimum coherent change.
5. Build the affected target.
6. Run the task's acceptance tests.
7. Run all earlier milestone gates that the change could regress.
8. Only then mark the task complete.

Do not batch ten speculative changes behind one build.

### Status vocabulary

Use only:

- `READY` — specified and unblocked, not implemented.
- `IN_PROGRESS` — source is actively being changed.
- `BLOCKED` — cannot proceed because a concrete external prerequisite is unavailable; include exact evidence.
- `GREEN` — implementation exists and acceptance checks passed.
- `RED` — implementation exists but acceptance checks fail.
- `VERIFIED-DESIGN` — design/API assumption verified, implementation not yet present.

Never call a task `GREEN` from inspection alone.

## Architecture boundaries

### AzerothCore / Playerbots fork

Owns engine, combat, maps, core scripting, and Playerbots-required core changes.

### mod-playerbots

Owns bot AI, random bot population, and alt-bot control.

### mod-individual-progression

Read-only integration from FURY's perspective. It may constrain per-character historical content eligibility but never owns household campaign state.

### mod-living-world

Owns invasion/stage/spawn/route/assault execution. The FURY bridge must stay narrow and must not introduce Household, Contracts, Chronicle, or Reward concepts into Living World.

### mod-fury

Owns:

- actor classification;
- household membership/state;
- durable normalized FURY events;
- event consumer checkpoints/replay;
- campaign graph/state;
- proof facts;
- reward claims/policy;
- Chronicle projection;
- contracts;
- Director runtime and outcomes;
- minimal Bestiary metadata/progression for M3;
- minimal profession orders for M3;
- adapters to external modules.

## M1–M3 non-goals

Do not add these before M3 is green:

- full Hunts integration;
- custom client assets or MPQ;
- Mythic+;
- Dungeon Master integration;
- Paragon / Infusion;
- full player housing/base interiors;
- Relic system;
- persistent Westfall occupation branch;
- broad economy redesign;
- procedural Director graph generation.

## Database rules

- Prefer a module-owned `acore_fury` database using AzerothCore `ModuleDatabasePool` + `DatabaseScript` lifecycle.
- Schema changes are forward migrations.
- Base schema must support a clean install.
- Update migrations must support upgrading from the previous project revision.
- Runtime rows with important state transitions use optimistic revision checks where appropriate.
- Durable event identities and reward claims require uniqueness constraints, not only application-level checks.
- Never use destructive startup SQL to “repair” production state silently.

## Event rules

Hook handlers are adapters, not business logic.

Correct:

`AzerothCore hook -> normalize -> append FuryEvent -> consumers`

Incorrect:

`OnPlayerCreatureKill -> update contract + reward + chronicle + director directly`

Consumers must tolerate replay.

## Bot policy

At minimum distinguish:

- Human;
- HouseholdAltBot;
- RandomPlayerBot;
- System.

Persistent FURY progression defaults to human authority only. If a later subsystem intentionally grants an exception, it must encode that exception explicitly and test it.

## Test gates

### Per-task

- affected unit/domain tests;
- compilation of affected target;
- schema validation when SQL changed.

### M1 gate

- clean module build;
- clean `acore_fury` creation;
- restart with existing schema;
- actor classification tests;
- event dedupe/replay tests;
- reward claim idempotency skeleton;
- Chronicle projection smoke;
- `.fury` diagnostic smoke.

### M2 gate

Everything from M1 plus campaign, proofs, contracts, Director, minimal Bestiary/profession logic, and IP adapter tests.

### M3 gate

Everything from M1/M2 plus Defias GS10–GS18 and one full worldserver vertical-slice acceptance run.

## Failure policy

When a build/test fails:

1. reproduce the exact failure;
2. identify whether it is FURY, upstream API mismatch, migration order, configuration, or environment;
3. patch the smallest responsible layer;
4. rerun the failed check;
5. rerun the relevant regression gate.

Do not “solve” build errors by deleting behavior or disabling tests without documenting why the original requirement is invalid.

## Upstream verification notes

As of the architecture pass on 2026-09-22:

- Playerbots requires the `mod-playerbots/azerothcore-wotlk` `Playerbot` fork.
- AzerothCore supports module-owned databases through `ModuleDatabasePool`/`DatabaseScript`.
- PlayerScript currently exposes relevant hooks including quest completion, creature kills, zone changes, level changes, and item creation/store callbacks.

These are design assumptions only until the exact pinned checkout is inspected.

## Definition of “done”

A feature is done only when:

- source exists;
- migrations/config exist if required;
- build is green;
- acceptance tests are green;
- restart/replay semantics are tested if the feature persists state;
- diagnostics are sufficient to inspect it on a live private server;
- documentation matches actual behavior.
