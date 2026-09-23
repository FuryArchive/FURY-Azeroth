# T33 — Director / Living World recovery

T33 executes the reconciliation plan that T16/T23 previously only modeled.

The recovery loop runs on the normal FURY reconcile cadence and treats the
Director row as durable campaign authority while Living World remains the
physical executor.

## Recovery cases

### Director active + Living World active, same runtime

No new invasion is started. FURY records a replay-safe
`director.recovery.verified` diagnostic event and leaves the executor alone.

### Director active + Living World active, different runtime id

This is the important crash-window case.

A replacement Living World runtime may have been started successfully before
FURY persisted the new binding. Because the adapter only inspects the single
managed invasion for this Director graph, T33 may safely adopt that active
replacement instead of creating another invasion.

The rebind is guarded by:

- Director run id;
- household id;
- expected revision;
- expected previous runtime id;
- active Director status.

A stale recovery cannot overwrite a newer binding.

### Director active + bound Living World runtime missing

T33 emits a durable recovery event, asks Living World to start the managed
Defias invasion once, then binds the returned replacement runtime through the
guarded Director recovery mutation.

If the server crashes after Living World starts but before FURY stores the new
runtime id, the next reconcile pass sees the active replacement and follows the
adoption path above. It does not start a third runtime.

### Director active + unbound + Living World active

The existing executor is attached to the run through the ordinary Director
attach path.

### Director terminal or missing + orphan Living World active

If there is no active Defias Director run but invasion 1 is still physically
active, T33 records an orphan-recovery event and calls Living World's
`FailRuntime` cleanup path.

The pinned FURY Living World bridge observes failure before cleanup. If durable
observation cannot be persisted, Living World leaves the runtime in place and
T33 retries later instead of deleting state blindly.

## Terminal executor observations

If Living World reports a terminal state while the Director is still active,
T33 does not independently resolve the campaign. Polling first writes the
normal durable Living World observation; the existing Defias graph and T32
resolution projection remain the only owners of phase/outcome semantics.

This avoids two competing recovery/resolution authorities.

## Observability

Recovery actions are visible through:

- `director.recovery.*` durable events;
- `director.runtime.recovered` when a runtime binding changes;
- explicit `[FURY Recovery]` server log messages.

Unresolved conflicts are logged and retried rather than silently overwritten.

## Validation

`scripts/test-t33-director-lw-recovery.sh` validates:

- the T16 reconciliation action matrix;
- replacement runtime adoption;
- guarded runtime replacement in the FURY database;
- replay-idempotent revision behavior;
- stale recovery protection;
- restart-missing wiring through `StartInvasion`;
- replacement binding through `RecoverRuntime`;
- orphan cleanup through `AbortRuntime` / Living World `FailRuntime`;
- recovery logging/diagnostic wiring.

The normal Fast mod-fury compile gate additionally verifies the concrete
AzerothCore/Living World API integration.
