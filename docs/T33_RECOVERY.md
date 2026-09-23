# T33 — Director / Living World recovery

T33 executes the reconciliation plan introduced in T16/T23. Recovery is
deliberately conservative: FURY never creates a replacement invasion merely
because a previously bound Living World runtime vanished.

## Recovery model

The 30-second reconcile cadence does two things:

1. polls all managed Living World runtimes into durable FURY observation events;
2. records a durable recovery intent for any Director/runtime mismatch.

The periodic scan itself does not mutate Director or Living World state.
`DefiasRecoveryService` is also an EventConsumer, so the intent is executed
through the normal at-least-once event stream. If the server crashes after a
mutation but before the consumer checkpoint advances, the same recovery event
is replayed and the existing idempotent Director/Living World boundaries repair
the incomplete operation.

## Cases

### Director active, Living World active, same runtime

Action: verified/reattached.

No new invasion is created. FURY writes one deduplicated
`director.recovery.verified` event for observability.

### Director active, Living World active, Director not yet bound

Action: attach the discovered runtime.

This is the crash gap after Living World successfully starts but before
`external_runtime_id` becomes durable on the Director run. The recovery event
replays `DirectorService::AttachRuntime`; a replay cannot replace a different
runtime id.

### Director active, previously bound Living World runtime missing

Action: safe abort with outcome `recovery.runtime_missing`.

FURY intentionally does **not** start a replacement runtime. The aborted run
frees the Director scope and the incomplete Westfall campaign can be attempted
again from a later legitimate Human trigger.

### Runtime id conflict

Action:

1. fail/clean the actual FURY-managed Living World runtime;
2. abort the Director run with `recovery.runtime_conflict`.

The Living World adapter refuses destructive failure for invasions that are not
registered as FURY-managed.

### External runtime Complete/Failed

The recovery scan does not invent a second terminal path. Polling has already
persisted the authoritative `living_world.runtime.observed` event, and the
normal Defias graph + T32 resolution consumers handle it.

### Managed Defias runtime with no active Director run

Because T24 disables random start for invasion 1, an active managed Defias
runtime without any active Defias Director run is an orphan. This includes:

- a terminal Director run whose external runtime survived;
- a missing Director row with a surviving FURY-owned runtime.

FURY queues `director.recovery.orphan_runtime`, then uses Living World's real
`FailRuntime` cleanup path. The patched completion observer persists the
terminal Living World observation before cleanup.

## Crash/replay behavior

Recovery event identities include run/action/runtime ids. Therefore:

- attach intent is unique;
- missing-runtime abort intent is unique;
- conflict cleanup intent is unique;
- orphan cleanup intent is unique;
- verified restart evidence is unique.

A recovery request that becomes stale because another authoritative path
already terminalized the Director run is consumed harmlessly rather than
blocking the event-stream checkpoint.

## Observability

Recovery mismatches are visible in two places:

- explicit WARN/ERROR server log messages;
- durable `director.recovery.*` events in the FURY event store, visible
  through existing event/diagnostic tooling.

## Validation

`scripts/test-t33-defias-recovery.sh` checks:

- T16 action-policy matrix with safe abort for a missing bound runtime;
- Living World terminal observer regression;
- crash-gap attach and replay;
- restart preserving the same runtime binding;
- no duplicate active Director run after restart;
- missing-runtime abort and scope release;
- later Human retry after safe abort;
- deterministic runtime-conflict outcome;
- replay-idempotent orphan cleanup intent;
- actual source wiring to Living World's `FailRuntime`;
- recovery cadence and log/event observability.
