# T25 — Defias Director graph

Implements the existing T25 scope on the accepted T16/T23/T24 boundaries.
The referenced IMPLEMENTATION_SPEC.md is absent from this repository; the
backlog and recovered accepted scenario settings govern this implementation.

## Behavior

A durable AzerothCore zone/login/level event with a human level snapshot >=10,
map 0, zone 40 and a household starts `classic.westfall.defias_resurgence.v1`
in `rumours`, unless Westfall campaign is complete or this household already
has a Defias run. Another active graph in `classic.westfall` owns the scope.
Bots cannot start or activate this graph. Old events without a level snapshot
fail closed; a fresh login/zone/level event supplies one.

Activation is configurable: named contract, live presence, or either. Default
presence is 120 seconds continuously observed online in Westfall. This is a
tuning default, not a previously agreed balance value. Disconnect, leaving,
invalid actor/level/membership, or restart resets an unqualified timer. An
already appended activation event survives restart. Offline time never counts.
The contract key is explicitly configured and reserved for T27; no board or
contract is invented by this step.

The graph persists `invasion` before calling LivingWorldAdapter, then binds the
runtime id. An interrupted binding retries through the adapter's idempotent
active-runtime path. Durable LW observations for the matching runtime move to
`final_battle` at authored stage 1006 and `resolution` on terminal execution.
T33 owns missing/conflicting external-runtime recovery. T29 supplies human
participation score; T32 applies Success/Partial/Ignored outcomes and rewards.
This change exposes the 70/30 threshold policy but does not manufacture score,
award rewards, or complete campaign merely because LW ended.

A Defias-specific generated unique key enforces one run per household even
once terminal. Other Director graphs retain repeatable behavior. Forward SQL
is repeatable and seeds without replacing operator content settings.

`.fury defias` reports enabled/content state and active run phase/runtime.
LW observation identity now includes state: completion on the same stage must
not collide with an earlier running observation.

## Validation

- `bash scripts/test-t25-defias-graph.sh`: actual graph controller exercised
  through an in-memory persistence/executor boundary; actor/level/location,
  duplicate and terminal starts, deferred activation, binding retry/restart,
  mismatched runtime, monotonic phase and outcome threshold scenarios.
- `bash scripts/test-t25-defias-schema.sh`: MySQL clean base, prior Director
  schema upgrade twice, duplicate and terminal re-entry, fresh-connection state.
- Existing M1/M2, LW boundary/content gates and Fast mod-fury compile in CI.
- Full worldserver acceptance remains T34; this is not yet a playable M3 slice.
