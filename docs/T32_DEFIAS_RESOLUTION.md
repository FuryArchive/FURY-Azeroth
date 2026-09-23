# T32 — Defias persistent resolution

T32 turns the scored Defias Director result into durable player-facing world
state. The canonical terminal event is now the actual
`director.run.resolved` event, and every downstream projection is replay-safe.

## Resolution flow

When the Defias graph enters the `resolution` phase, the T32 consumer reads
the T29 score for that exact Director run and resolves it as:

- Success: score >= configured success threshold;
- Partial: score >= configured partial threshold but below Success;
- Ignored: score below the Partial threshold.

Director persistence first records the terminal state and outcome, emits the
deduplicated `director.run.resolved` event, and then binds that emitted event
id back into `fury_director_run.resolved_event_id`.

This fixes an earlier semantic mismatch where the column could point at the
Living World observation that triggered resolution rather than the canonical
Director terminal event.

## Success

Success persists:

- Chronicle: **Westfall defended**;
- household proof: `world.westfall.defended`;
- `campaign.classic.westfall` becomes Complete;
- one household reward entitlement:
  `classic.westfall.defias.success`.

The reward entitlement is intentionally a Pending claim with no physical
reward entries yet. T32 records the right to the reward but withholds valuable
item delivery until FURY has receipt-backed materialization, preventing replay
from ever duplicating physical loot.

## Partial

Partial persists:

- Chronicle: **Westfall bloodied**;
- household proof: `world.westfall.bloodied`;
- campaign remains Active.

A later eligible Human event may start a new Defias Director run. Replaying the
original start event cannot create another run.

## Ignored

Ignored persists:

- Chronicle: **Westfall crisis ignored**;
- emergency content flag/proof: `world.westfall.emergency`;
- one replay-safe `defias.pressure.increased` event;
- campaign remains Active.

No vanilla quest state is mutated.

## Human authority reconstruction

Living World lifecycle events are System-authored, so the terminal Director
event can also be System-authored. Persistent Campaign and Proof services only
accept Human authority.

T32 therefore loads the durable Human event that originally started the
Director run and uses that actor identity when applying household progression.
The terminal event remains the source event id for the actual outcome
projection.

## Replay properties

All permanent effects have stable identities or unique persistence keys:

- Director terminal event: one per run;
- Proof: one household/proof key;
- Chronicle: one entry/source event projection;
- reward claim: one source/reward/beneficiary tuple;
- pressure increase: one event per run.

Partial and Ignored terminal runs no longer softlock the campaign. A genuinely
later eligible event may create another run because the previous scope is
terminal and the campaign is still incomplete.

## Validation

`scripts/test-t32-defias-resolution.sh` covers:

- canonical terminal event binding;
- Success proof/campaign/reward/Chronicle persistence;
- Partial proof/Chronicle and campaign continuation;
- Ignored emergency flag, pressure event and Chronicle;
- replay idempotency for every outcome;
- physical reward withholding;
- a later Director attempt after Partial;
- the existing T25 duplicate-start and graph-recovery regression suite.
