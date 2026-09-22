# T21 — Living World / Defias pinned-source audit

Date: 2026-09-23

This audit is authoritative for M3 and is tied to the exact Living World pin in `vendor/lock/fury.lock.yaml`.

## Exact upstream

- Repository: `Hisha/mod-living-world`
- Branch metadata: `main`
- Commit: `116926ef9ce42ee0bba223c1e406cf62cedd9904`
- Defias source: `data/sql/db-world/prebuilt/900_defias_westfall_invasion.sql`

No identifier below is copied from an old design document; all values were re-read from the pinned source/SQL.

## Defias authored identifiers

### Invasion

- invasion id: `1`
- name: `Defias Westfall Invasion`
- map: `0`
- zone: `40` (Westfall)
- recommended level: `10–20`
- hard runtime: `3600s`
- `allow_random_start = 1` at the upstream pin
- enabled: `1`

FURY M3 must own an overlay that changes `allow_random_start` to `0` before Director control is enabled.

### Stages

| ID | Order | Name | Completion |
|---:|---:|---|---|
| 1001 | 10 | Defias Scouts | runtime signal 100 |
| 1002 | 20 | Defias Populate Staging | timer 10s |
| 1003 | 30 | Defias Establish Control | timer 600s |
| 1004 | 40 | Defias Leadership Arrives | timer 10s |
| 1005 | 50 | Stormwind Response | runtime signal 103 |
| 1006 | 60 | Stormwind vs Defias | runtime signal 104 |

Pinned runtime code implements completion type `0 = timer` and `1 = runtime signal`. Objective/manual completion types are not implemented in this runtime.

### Runtime signals

| ID | Name | Pinned Defias use |
|---:|---|---|
| 100 | ScoutRouteComplete | stage 1001 completion |
| 101 | StagingComplete | authored definition exists, but no pinned Defias stage waits on it |
| 102 | LeadershipComplete | authored definition exists, but no pinned Defias stage waits on it |
| 103 | StormwindResponseComplete | stage 1005 completion |
| 104 | StormwindWins | stage 1006 completion |

Signals 101 and 102 must not be treated as required stage gates by FURY unless the upstream content is deliberately changed later.

### Spawn groups

| ID | Name |
|---:|---|
| 100 | Defias Scouts |
| 101 | Defias 2nd Scouts |
| 102 | Defias Control Team |
| 103 | Defias 2nd Control Team |
| 104 | Defias 3rd Control Team |
| 105 | Defias Leadership |
| 106 | Stormwind Response Force |
| 107 | Stormwind Sentinel Hill Inn Detachment |

The final stage registers defeat watches for Defias groups 100–105 with signal 104 and `require_all = true`. `GroupDefeatWatcher` aggregates those registrations into one runtime watch, so signal 104 is emitted only when all watched Defias groups are defeated.

## Public API boundary at the pin

### Correct controlled-start API

`InvasionScheduler::TriggerInvasion(uint32 invasionId)` is public and delegates to the scheduler's private `StartInvasion(...)`.

Use this API from the FURY adapter. Do **not** call `InvasionRuntimeManager::StartInvasion()` directly for a Director-controlled start because the scheduler owns persistent Available/Active/Cooldown state.

### Runtime queries

`InvasionRuntimeManager` publicly exposes:

- `GetActiveRuntimeCount()`
- `GetRuntimeForInvasion(invasionId)`
- `GetRuntime(runtimeId)`

`InvasionRuntime` publicly exposes the runtime id, invasion id, current stage, stage timestamps and runtime state.

### Runtime signals

`RuntimeSignalManager` publicly exposes:

- `EmitSignal(runtimeId, signalId)`
- `HasSignal(runtimeId, signalId)`
- `GetSignals(runtimeId)`

`EmitSignal` is idempotent for one `runtimeId + signalId`.

### Authored data

`LivingWorldDataMgr` publicly exposes invasion definitions, stages, stage actions, spawn groups, spawn members, route data and runtime signal definitions. FURY startup validation does not need direct world-DB reads.

### Runtime entity groups

`RuntimeEntityGroupManager` publicly exposes group lookup and runtime-to-group enumeration. Each group contains:

- `RuntimeId`
- authored `SpawnGroupId`
- the runtime entity list

Each `RuntimeEntity` includes its `ObjectGuid`, member id, entry, LW template id and tactical role.

There is no direct public reverse lookup from a killed creature GUID to its runtime/spawn-group metadata. This is the one concrete API gap required by M3 event classification.

## Persistence and restart semantics

Living World has two persistent character-DB layers:

1. `lw_invasion_runtime`: scheduler state (Available/Active/Cooldown, last start/completion, next eligible time, active interval, counters).
2. `lw_active_runtime`: active runtime id, invasion id, current stage id and stage/runtime timestamps.

On startup, `InvasionRuntimeManager::LoadActiveRuntimes()` restores a runtime only if:
- the authored invasion/stages still exist; and
- the scheduler still marks that invasion Active.

If scheduler and active-runtime state disagree, Living World repairs toward a safe non-active state.

### Important signal limitation

Runtime signals are in-memory only. `RuntimeSignalManager::Reset()` clears them and there is no signal persistence table.

Consequence for FURY:
- an objective-complete fact belongs durably to FURY;
- if FURY emitted a signal and the server crashes before LW consumes it, the signal may be lost;
- during reconciliation, if the Director objective is already complete and LW is still on the matching signal-gated stage, FURY may safely re-emit that signal;
- this is safe because signal emission is idempotent within the runtime.

## Lifecycle observation decision

The pinned module has no generic stage/runtime callback/listener surface.

M3 does not need to add one. FURY can use the existing query API plus its throttled reconciler to observe:
- whether invasion 1 has an active runtime;
- its runtime id;
- its current stage;
- scheduler/runtime divergence.

This avoids adding an unnecessary cross-module callback framework.

## T22 bridge delta

After this audit, the upstream patch should be smaller than originally planned.

Already public; do not patch:
- Director-safe controlled start via `InvasionScheduler::TriggerInvasion()`;
- runtime lookup;
- current-stage lookup;
- runtime signal emission/query;
- authored-data validation.

Patch only the missing stable reverse lookup:
- killed/spawned creature GUID -> runtime id + runtime group id + authored spawn group id (+ existing entity metadata as useful).

Also add a pin/API compatibility guard so the FURY patch fails loudly if the upstream structures it depends on change.

## T21 result

T21 acceptance is satisfied:
- exact Defias invasion/stage/signal/spawn-group identifiers are captured from pinned source;
- runtime persistence/restart semantics are documented from code + schema;
- existing public API boundaries are re-verified;
- stale bridge assumptions are corrected before M3 code starts.
