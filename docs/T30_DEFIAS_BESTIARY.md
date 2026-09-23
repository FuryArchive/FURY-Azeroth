# T30 — Defias Bestiary content

T30 adds first-party Bestiary progression for the Defias Resurgence vertical
slice without depending on ordinary Westfall creature entries or mod-hunts.

## Entries

Three account-scoped entries are seeded:

- `classic.westfall.defias.scouts` — Defias Scouts;
- `classic.westfall.defias.control_teams` — Defias Control Teams;
- `classic.westfall.defias.commander` — Captain Garrick Vane.

## Runtime-only mapping

T30 adds `fury_bestiary_event_map`, a generic Bestiary mapping keyed by:

- event type;
- subject type;
- subject id;
- target Bestiary entry;
- discovery level.

Defias content maps only the pinned Living World hostile spawn groups:

- 100/101 -> Defias Scouts;
- 102/103/104 -> Defias Control Teams;
- 105 -> Captain Garrick Vane.

All six mappings promote to `Studied`.

No `fury_bestiary_creature_map` row is created for any Defias Bestiary
entry. This is deliberate: ordinary entry 449/589 creatures elsewhere in
Westfall cannot advance the Defias invasion Bestiary.

## Personal participation event

T28's `living_world.entity.killed` event is intentionally household-scoped:
only one durable kill fact per physical runtime entity and participating
household is emitted so Contracts cannot double-count.

Bestiary is account-scoped, so T30 adds a second durable fact:

`defias.bestiary.entity.participated`

It is emitted once per eligible Human account that T28 actually resolved as a
Direct or legitimate GroupShare participant. The dedupe identity is:

`runtime + runtime-group + creature-guid + account-id`

The event carries the authored Living World spawn-group id and is the only
Defias runtime event consumed by the Bestiary mapping.

This keeps the two semantics separate:

- Contracts / score: household-scoped;
- Bestiary: personal Human participation.

A bot-only kill still produces no personal Bestiary credit.

## Commander mastery

A runtime group-105 participation promotes Captain Garrick Vane to
`Studied` for that Human account.

`Mastered` requires both:

1. that account already has the commander at least `Studied`;
2. the household's canonical Defias Director run is durably
   `Complete` with `outcome_key = success`.

The Bestiary consumer handles the durable `director.run.resolved` event,
re-reads the persisted Director run, then queries only household member
accounts already at `Studied` for the commander. Those accounts are promoted
to `Mastered`.

A household member who never studied the commander is not granted mastery just
because another member did.

The normal Bestiary last-event guard and stable
`bestiary:advance:v1:<account>:<entry>:<level>` event identity make outcome
replay idempotent.

## No mod-hunts dependency

T30 has no dependency on mod-hunts, trophy drops, or later hunt content.
Those remain deferred to M4.

## Validation

`scripts/test-t30-defias-bestiary.sh` verifies:

- event-map schema exists and is idempotent;
- exactly three Defias Bestiary entries are seeded;
- all six hostile runtime groups map explicitly to Studied;
- no ordinary creature-entry mapping exists for Defias entries;
- runtime group 105 studies Captain Garrick Vane;
- another household member without participation remains Unknown;
- success selects only household accounts that already studied the commander;
- successful outcome promotes the studied commander to Mastered;
- replaying the same success event does not mutate mastery twice;
- a non-participating household member is not granted mastery.

Fast `mod-fury` compile validates the BestiaryService /
DirectorRepository / ParticipationService integration against the pinned
AzerothCore API.
