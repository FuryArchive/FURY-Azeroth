# T27 — Six Defias contracts

T27 turns the T26 board into actual Defias Resurgence gameplay without
classifying ordinary Westfall creatures as invasion targets.

## Runtime kill boundary

AzerothCore still emits its normal `creature.killed` event. In parallel, the
FURY player collector asks `LivingWorldAdapter::FindEntity(creatureGuid)`.
Only when the killed GUID belongs to the currently active pinned Defias
runtime, and its authored spawn group is one of 100–105, does FURY emit:

`living_world.entity.killed`

The event subject is:

- `subject_type = living_world_spawn_group`
- `subject_id = authored spawn group id`

Its payload retains runtime id, runtime-group id, spawn member id, creature
entry/template, tactical role, physical GUID and pet-owner attribution.

The dedupe identity is runtime + runtime-group + physical creature GUID, so a
duplicate direct/pet callback for the same spawned entity cannot increment a
contract twice.

Stormwind groups 106/107 are explicitly excluded from hostile classification.
Ordinary world Defias with the same creature entry never receive the runtime
event and therefore cannot satisfy T27 objectives.

The pinned Living World defeat watcher does not erase runtime group metadata
on the creature death callback. It polls group members and `IsAlive()` once
per second; entity groups survive until runtime cleanup. That keeps reverse
metadata lookup available to the FURY PlayerScript kill hook.

## Replay cutoff

Contract objective matching now requires:

`event.id >= contract_instance.accepted_event_id`

This closes a generic replay edge: even if a consumer checkpoint is rebuilt
far behind current state, historical events from before the contract was
accepted cannot progress the active instance.

For `living_world.entity.killed`, the objective query also joins the
contract's `director_run_id` and requires the event payload's `runtime_id`
to equal that Director run's attached `external_runtime_id`. A manually
started/conflicting Living World runtime therefore cannot feed a different
FURY Director contract.

## Initial contracts

### Recon Roads

Key: `classic.westfall.defias.scout_report`

Phase: `rumours`

This deliberately keeps the activation-contract key already configured by
T25. Accepting it queues the durable Defias activation decision. Its gameplay
objective then requires one human-authored runtime kill from each scout group:

- group 100 — Defias Scouts;
- group 101 — Defias 2nd Scouts.

### Break Scouts

Phase: `invasion`

Requires three human-authored runtime kills from group 100 and three from
group 101. The pinned content authors exactly three creatures in each scout
group.

### Break Control

Phase: `invasion`

Requires six human-authored runtime kills from each control group:

- 102 — Defias Control Team;
- 103 — Defias 2nd Control Team;
- 104 — Defias 3rd Control Team.

Each pinned group contains 30 authored creatures, so the contract asks the
players to damage all three formations rather than farm one group.

### Hold Sentinel

Phase: `invasion`

Broad participation contract across all hostile formations:

- groups 100/101: 1 each;
- groups 102/103/104: 3 each;
- group 105: 1.

T28 may later credit legitimate nearby human participation when a bot lands
the final blow; T27 itself never grants progress from a bot-only kill.

### Field Relief

Phase: `invasion`

The contract exists now with one stable completion signal:

`defias.field_relief.completed / defias_contract_signal:1`

T31 owns the adaptive profession-order implementation that emits that signal
after verifying actual profession/item ids. T27 intentionally does not invent
those ids. Field Relief remains optional.

### Defeat Commander

Phase: `invasion`

Requires one human-authored runtime kill from group 105, the pinned Defias
Leadership group containing Captain Garrick Vane.

## Validation

`scripts/test-t27-defias-contracts.sh` checks:

- exact hostile/scout/control/commander group classification;
- all six definitions and 15 objectives;
- no T27 objective listens to ordinary `creature.killed`;
- 14 kill objectives target only verified groups 100–105;
- Field Relief is explicitly delegated to T31;
- repeatable seed migration;
- pre-acceptance runtime events cannot progress a contract after replay;
- ordinary entry-449 Defias kills do not match;
- a post-acceptance runtime group-100 kill does match;
- a kill from a different external runtime does not match.

Fast `mod-fury` compilation validates the PlayerScript → LivingWorldAdapter
→ EventStore integration against the exact pinned AzerothCore/Living World
headers.
