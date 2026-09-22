# Upstream API Notes — T00.2

Date: 2026-09-22

These notes are tied to the exact revisions in `vendor/lock/fury.lock.yaml`. They are not generic AzerothCore assumptions.

## 1. Playerbots real-player detection

Pinned module:

- Repository: `mod-playerbots/mod-playerbots`
- Commit: `7bae1b5c58c76a0aa20381155edc08096d1485b2`

Verified file:

`src/Bot/PlayerbotAI.h`

Verified declaration:

```cpp
bool IsRealPlayer(Player* player);
bool IsSelfBot(Player* player);
```

Implementation exists in `src/Bot/PlayerbotAI.cpp`.

FURY consequence:

- `ActorResolver` may use `IsRealPlayer(Player*)` directly.
- Do not infer bot state from account names, session flags, character naming conventions, or random-bot tables.
- HouseholdAltBot vs RandomPlayerBot remains a FURY distinction layered on top of Playerbots' real-player detection.

## 2. Module-owned database lifecycle

Pinned core:

- Repository: `mod-playerbots/azerothcore-wotlk`
- Branch: `Playerbot`
- Commit: `7f12e89ee5f467a50e62eba1d525eac7dc953d03`

Verified file:

`src/server/game/Scripting/ScriptDefines/DatabaseScript.h`

Verified hooks:

```cpp
virtual bool OnModuleDatabasesLoading();
virtual void OnModuleDatabasesKeepAlive();
virtual void OnModuleDatabasesClosing();
virtual void OnDatabaseWarnAboutSyncQueries(bool apply);
virtual void OnDatabaseGetDBRevision(std::map<std::string, std::string>& revisions);
```

The exact core dispatch is implemented in:

`src/server/game/Scripting/ScriptDefines/DatabaseScript.cpp`

A concrete custom module database implementation at the selected module pin exists in:

`mod-playerbots/src/Script/Playerbots.cpp`

Its `PlayerbotsDatabaseScript`:

- reads its own connection string;
- opens a module database;
- creates a missing database through `ModuleDBUpdater::Create`;
- populates and updates through `ModuleDBUpdater`;
- prepares statements;
- handles keep-alive and close;
- reports DB revision.

FURY consequence:

T02 should follow this selected-pin pattern instead of inventing a separate bootstrap mechanism.

## 3. PlayerScript hooks required by the Event Spine

Pinned file:

`src/server/game/Scripting/ScriptDefines/PlayerScript.h`

Verified hooks/signatures include:

```cpp
virtual void OnPlayerCompleteQuest(Player*, Quest const*);
virtual void OnPlayerCreatureKill(Player*, Creature*);
virtual void OnPlayerCreatureKilledByPet(Player*, Creature*);
virtual void OnPlayerLevelChanged(Player*, uint8 oldLevel);
virtual void OnPlayerLogin(Player*);
virtual void OnPlayerUpdateZone(Player*, uint32 newZone, uint32 newArea);
virtual void OnPlayerUpdateArea(Player*, uint32 oldArea, uint32 newArea);
```

The pinned hook enum also includes:

```text
PLAYERHOOK_ON_LOOT_ITEM
PLAYERHOOK_ON_STORE_NEW_ITEM
PLAYERHOOK_ON_CREATE_ITEM
PLAYERHOOK_ON_UPDATE_SKILL
```

Therefore the planned M1/M2 Event Spine does not require a core patch.

Implementation rule remains:

```text
AzerothCore hook
→ normalize FuryEvent
→ durable append
→ consumers
```

No Contract/Reward/Chronicle business logic belongs directly in these hooks.

## 4. World lifecycle

Pinned file:

`src/server/game/Scripting/ScriptDefines/WorldScript.h`

Verified lifecycle:

```cpp
virtual void OnUpdate(uint32 diff);
virtual void OnStartup();
virtual void OnShutdown();
```

The source explicitly warns not to execute heavy code on every world tick.

FURY consequence:

`FuryApp::Update` will use throttled accumulators/queues rather than table scans or Director evaluation every core tick.

## 5. Module loader convention

Verified Living World file at the pinned revision:

`src/mod_living_world_loader.cpp`

Pattern:

```cpp
void AddLivingWorldScripts();

void Addmod_living_worldScripts()
{
    AddLivingWorldScripts();
}
```

T01 will inspect another minimal module at the resolved checkout if needed, but the required generated module-entry naming convention is now verified against an actual pinned module.

## 6. Living World runtime API

Pinned module:

- Repository: `Hisha/mod-living-world`
- Commit: `116926ef9ce42ee0bba223c1e406cf62cedd9904`

### Invasion runtime

Important T21 correction/refinement: the preferred controlled-start API is public `InvasionScheduler::TriggerInvasion(uint32 invasionId)`, which updates scheduler persistence and delegates to the runtime manager. FURY must use the scheduler entrypoint rather than calling the runtime manager directly.

Verified file:

`src/invasions/InvasionRuntimeManager.h`

Public methods already include:

```cpp
bool StartInvasion(uint32 invasionId);
bool AdvanceRuntime(uint64 runtimeId);
bool FailRuntime(uint64 runtimeId, char const* reason);

uint32 GetActiveRuntimeCount() const;
InvasionRuntime const* GetRuntimeForInvasion(uint32 invasionId) const;
InvasionRuntime const* GetRuntime(uint64 runtimeId) const;
```

This changes the initial bridge estimate: controlled start and runtime lookup already exist publicly.

### Runtime signals

Verified file:

`src/core/RuntimeSignalManager.h`

Public methods:

```cpp
bool EmitSignal(uint64 runtimeId, uint32 signalId);
bool HasSignal(uint64 runtimeId, uint32 signalId) const;
std::vector<RuntimeSignal> GetSignals(uint64 runtimeId) const;
void ClearRuntime(uint64 runtimeId);
```

Therefore signal sending also already exists publicly.

### Runtime entity groups

Verified file:

`src/core/RuntimeEntityGroup.h`

Public data already records:

```cpp
uint64 RuntimeId;
uint32 SpawnGroupId;
std::vector<RuntimeEntity> Entities;
```

and the manager exposes runtime/group lookup APIs.

The exact reverse lookup from a killed `Creature` to runtime/spawn-group metadata is not yet exposed as a single public function. That remains a legitimate candidate for the narrow M3 bridge patch.

### Living World authored data

Verified file:

`src/core/LivingWorld.h`

Public `LivingWorldDataMgr` can already query:

- invasion definitions;
- stages;
- actions;
- spawn groups;
- spawn members;
- runtime signal definitions.

This is sufficient for FURY startup validation without direct SQL reads.

## 7. Living World stage completion limitation

Pinned schema/documentation confirms:

```text
0 = timer
1 = runtime signal
2 = objective (reserved/unimplemented)
3 = manual (reserved/unimplemented)
```

FURY consequence:

- Contracts and authored FURY objectives remain in `mod-fury`.
- When a FURY objective should advance an LW stage, FURY emits one deterministic LW runtime signal.
- Do not embed FURY Contract logic into Living World.

## 8. Defias identifiers at the pin

Verified source:

`data/sql/db-world/prebuilt/900_defias_westfall_invasion.sql`

Current identifiers:

```text
Invasion
1  Defias Westfall Invasion

Stages
1001 Defias Scouts
1002 Defias Populate Staging
1003 Defias Establish Control
1004 Defias Leadership Arrives
1005 Stormwind Response
1006 Stormwind vs Defias

Spawn groups
100 Defias Scouts
101 Defias 2nd Scouts
102 Defias Control Team
103 Defias 2nd Control Team
104 Defias 3rd Control Team
105 Defias Leadership
106 Stormwind Response Force
107 Stormwind Sentinel Hill Inn Detachment

Runtime signals
100 ScoutRouteComplete
101 StagingComplete
102 LeadershipComplete
103 StormwindResponseComplete
104 StormwindWins
```

The prebuilt invasion currently has:

```text
allow_random_start = 1
```

M3 will apply a FURY-owned overlay changing this to `0`.

## 9. Current bridge delta after source verification

The original design assumed Living World needed a larger bridge.

After inspecting the pinned source, the required patch is smaller.

Already public:
- start invasion;
- find runtime for invasion;
- find runtime by id;
- emit runtime signal;
- query authored definitions/stages/groups/signals.

Still to implement for M3:
- stable reverse lookup from runtime-spawned `Creature`/GUID to runtime id + authored spawn group id.

T21 selected the polling/reconciliation alternative for lifecycle/stage observation. Existing public runtime/current-stage query APIs are sufficient, so no lifecycle callback framework should be added.

T21 also verified that runtime signals are in-memory only; FURY reconciliation must re-emit a durable objective's signal when needed after restart. `EmitSignal` is idempotent within one runtime.

Policy:

Prefer the smallest patch that exposes missing metadata. Do not patch APIs that already exist.

## 10. T00.2 result

Verified against exact pins:

- Playerbots real-player API;
- DatabaseScript lifecycle;
- required PlayerScript hooks;
- WorldScript lifecycle;
- module loader convention;
- Living World start/query/signal APIs;
- Living World Defias ids/signals/spawn groups;
- Living World completion-mode limitation.

Remaining compatibility proof is compilation/runtime, not API discovery. That is part of T01/T02 and the M1 gate.
