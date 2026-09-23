# T29 — Defias participation score

T29 turns recorded M3 achievements into a durable 0–100 Director score. It does
not resolve the Director run yet; T32 consumes this score and persists the
Success / Partial / Ignored outcome.

## Accepted score v1

| Component | Source | Points |
|---|---|---:|
| Recon | Recon Roads completed | 10 |
| Scouts | Break Scouts completed | 10 |
| Control | Break Control completed | 15 |
| Hold Sentinel | Hold Sentinel completed | 15 |
| Field Relief | Field Relief completed | 10 |
| Captain | Defeat Commander completed | 25 |
| Final-stage presence | Human participation in Living World stage 1006 | 15 |
| **Total** |  | **100** |

The six contract components contribute 85 points. Field Relief remains
optional; without it the theoretical maximum is still 90, so profession
availability cannot softlock Success.

Default thresholds:

- Success: score >= 70;
- Partial: score >= 30 and < 70;
- Ignored: score < 30.

Optional server tuning:

```ini
Fury.Defias.Score.PartialThreshold = 30
Fury.Defias.Score.SuccessThreshold = 70
```

Invalid threshold ordering disables scoring rather than silently changing
outcome semantics.

## Data-driven definitions

The generic `fury_director_score_component` table maps:

- Director graph;
- component key;
- score value;
- source event type;
- source correlation key.

The Defias v1 seed uses `INSERT IGNORE`, so operator tuning is not overwritten
by a migration re-run.

At startup, the Defias ScoreService requires the enabled component definitions
to total exactly 100. A malformed/tuned set that no longer totals 100 fails
closed until intentionally corrected.

## Immutable awards

`fury_director_score_award` stores one row per
`director_run_id + component_key`.

An award snapshots:

- graph;
- component;
- score value at award time;
- authoritative source event id.

Therefore:

- replaying the same completion cannot add the component again;
- a second event for the same component cannot add it again;
- later tuning of the definition value does not rewrite an existing run;
- score is deterministic for the persisted component set.

## Event sources

Six components consume normal `contract.completed` events and match the
contract key in `correlation_key`.

The seventh component consumes:

`defias.final_stage.participated`

T28 emits that event only from a resolved Human household participation credit
while the active Defias Living World runtime is in authored stage 1006. The
dedupe identity is one event per runtime + household, so repeated kills in the
final stage do not award the 15 points more than once.

## Event consumer

`Defias::ScoreService` is a replayable EventConsumer
(`defias.score.v1`). For each matching event it resolves the exact Director run from authoritative
event identity rather than "latest run" state:

- `contract.completed` must come from `fury.contracts`, point at the
  canonical contract instance, and match that instance's persisted
  `completed_event_id`;
- `defias.final_stage.participated` must come from `fury.defias` and point
  at the Living World runtime attached to the Director run.

The service then inserts the component award idempotently and verifies the
award is persisted. A similar-looking synthetic/duplicate event cannot be
attributed to the run.

The service exposes current score and threshold-derived outcome preview.
`.fury defias` reports both while the run is active.

## Validation

`scripts/test-t29-defias-score.sh` verifies:

- generic score tables exist;
- seven Defias definitions total exactly 100;
- accepted weights are 10/10/15/15/10/25/15;
- six contracts contribute 85;
- optional Field Relief is not required to make Success reachable;
- final-stage participation is exactly 15;
- a repeated component event awards once;
- the first source event remains authoritative;
- a non-canonical duplicate contract completion cannot resolve a score run;
- final-stage score resolves only through the attached external runtime;
- later definition tuning cannot rewrite an award snapshot;
- migration re-application preserves awards and definitions;
- final-stage participation is tied to stage 1006.

Fast `mod-fury` compile validates ScoreService, repository, EventBus,
participation-event and diagnostics integration against the pinned runtime.
