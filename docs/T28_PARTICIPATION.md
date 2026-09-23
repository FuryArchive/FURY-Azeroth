# T28 — Human participation resolver

T28 replaces last-hit ownership with encounter participation for the Defias
Living World runtime.

## Why

T27 intentionally classified only verified Living World runtime entities, but
its first implementation published the runtime kill from PlayerScript kill
callbacks. That is insufficient once bots participate in the battle: a bot
landing the final blow must not erase a real human's legitimate contribution,
and a household bot must not occupy the durable dedupe identity before the
human credit is resolved.

T28 moves authorship to a dedicated in-memory participation service driven by
AzerothCore UnitScript hooks.

## Direct participation

`UnitScript::OnDamage` resolves the attacker through
`GetCharmerOrOwnerPlayerOrPlayerItself()`. This means a player's pet,
guardian, charm or the player themself all resolve to the same Player owner.

The service records an encounter observation only when:

- the victim GUID resolves through LivingWorldAdapter;
- the entity belongs to the active pinned Defias runtime;
- its authored spawn group is hostile group 100–105;
- the resolved actor is Human and belongs to a FURY household;
- non-zero damage was dealt.

Bots therefore never become direct persistent participation authorities.

## Death resolution

`UnitScript::OnUnitDeath` resolves the same runtime entity and evaluates the
recorded human observations.

Defaults are tuning values, not permanent balance:

- participation window: 20 seconds;
- participation radius: 60 yards;
- same-group sharing: enabled.

Optional config overrides:

```ini
Fury.Defias.ParticipationWindowSeconds = 20
Fury.Defias.ParticipationRadiusYards = 60
Fury.Defias.ParticipationGroupShare = 1
```

A direct participant must still be online, in the same map/phase, within the
configured radius, and inside the action window when the creature dies.

When group sharing is enabled, an online Human group member inside the same
radius may inherit credit from a recent direct participant only if that direct
participant is still in the same group recorded at action time. A random nearby
non-grouped player receives nothing.

Direct credit outranks inherited group credit.

## Durable event

After resolving participation, FURY emits at most one
`living_world.entity.killed` event per credited household for that physical
runtime entity.

The event retains:

- runtime id and runtime-group id;
- authored spawn group/member/template metadata;
- creature GUID and final-blow GUID;
- final-blow ActorKind;
- canonical credited Human player;
- `credit_kind = direct | group_share`;
- number of recent direct human participants.

The dedupe identity is now household-scoped:

`runtime + runtime-group + creature-guid + household-id`

This preserves one progression increment per household while allowing two
different legitimately participating households to both receive credit.

## Bot final blow

The final blow no longer decides persistent authority. If a bot kills an
invasion creature after a recent valid Human contribution, the Human household
still receives the runtime kill event. A bot-only kill with no valid human
encounter observation produces no persistent runtime kill event.

## Lifecycle

Participation observations are intentionally transient. They are pruned after
the configured action window, removed on death, and cleared on FURY shutdown.
The durable result is the normalized kill event, not the transient combat map.

## Validation

`scripts/test-t28-participation.sh` covers:

- direct Human participation;
- pet/guardian owner normalization contract;
- bot-only rejection;
- random nearby non-participant rejection;
- group sharing enabled/disabled;
- time-window cutoff;
- radius cutoff;
- Direct priority over GroupShare;
- UnitScript damage/death hook wiring;
- removal of legacy PlayerScript runtime last-hit authorship;
- household-scoped v2 event identity.

Fast `mod-fury` compilation verifies the concrete UnitScript, Group,
ObjectAccessor, LivingWorldAdapter and Player APIs against the pinned
AzerothCore revision.
