# T34 — Defias vertical-slice acceptance runbook

T34 is the final M3 acceptance. The automated GS10-GS18 gate proves policy,
persistence and replay behavior. This runbook proves the same slice in a real
worldserver with two Human accounts, Playerbots and Living World active.

Do not edit FURY tables during this run.

## Preconditions

The server must use the pinned FURY baseline stack and the current `main`:

- Playerbots;
- Individual Progression;
- Living World with the FURY bridge;
- mod-fury;
- FURY world-DB overlays applied.

Both Human characters must be level 10 or higher.

At least one of the two characters must know one supported Field Relief path:

- Alchemy;
- First Aid;
- Cooking.

Before beginning, run from a GM account:

```text
.fury validate
.fury status
.fury defias
```

Acceptance begins only if validation reports HEALTHY and Defias reports
`enabled=yes content_valid=yes scoring=yes`.

## 1. Create the two-account household

Log in both Human accounts.

On the first Human character:

```text
.fury household create fury-t34
```

Select the second Human player and run:

```text
.fury household add
```

Verify on both Humans:

```text
.fury household status
.fury actor
```

Expected:

- household has 2/2 members;
- both accounts resolve as Human;
- both show the same household id.

Household alt-bots may participate in combat, but they are not household
members in the two-Human identity model and must not author Human-only
persistent credit.

## 2. Enter Westfall and create Rumours

Bring an eligible Human into Westfall (map 0, zone 40).

Use:

```text
.fury defias
.fury event tail 20
```

Expected:

- exactly one active Defias Director run;
- graph is `classic.westfall.defias_resurgence.v1`;
- initial phase is `rumours`;
- repeated movement/login/zone events do not create a second active run.

## 3. Use the Westfall contract board

The FURY board is spawned at:

```text
map 0 / zone 40 / area 108
x = -10651.789063
y =  1039.241821
z =    33.536880
```

It is the server-side **FURY Westfall Contract Board** and does not require a
client addon.

Open it with a Human character.

Acceptance requires:

- board opens normally;
- contracts show Available/Active/Complete state;
- accept at least one ordinary Defias contract;
- accept **Field Relief** on the Human who knows Alchemy, First Aid or Cooking.

The default activation contract is Recon Roads. Accepting it is the simplest
way to advance the Director into the invasion without waiting for the presence
timer.

## 4. Verify Living World activation

After activation:

```text
.fury defias
.fury event tail 30
```

Expected:

- Director phase reaches `invasion`;
- one Living World runtime id is attached;
- invasion id 1 is under FURY control;
- there is no second concurrent Defias runtime.

This is the point where Playerbots may be grouped with the Humans.

## 5. Fight the real invasion

Play the invasion normally with both Humans and any desired Playerbots.

During the run verify the following by actual gameplay:

- Humans damage/participate in Defias runtime entities;
- bots may assist, tank, heal and kill;
- bot-only activity does not create Human participation credit;
- Human participation generates Defias scoring;
- complete at least one accepted contract;
- craft the Field Relief target until its Profession Order completes;
- progress at least one Defias Bestiary entry to Studied or higher.

Useful diagnostics:

```text
.fury defias
.fury event tail 50
```

The run is invalid if persistent progress is awarded only from random bots or
if ordinary non-runtime Westfall Defias advance the T30 Bestiary entries.

## 6. Finish the Living World final stage

Complete the Living World invasion through its final stage.

Wait for FURY event replay to settle, then run:

```text
.fury defias
.fury event tail 50
.fury reward claims 20
```

Expected:

- no active Defias run remains;
- Director has one canonical `director.run.resolved`;
- the resulting score maps to exactly one of Success / Partial / Ignored.

Outcome consequences:

| Outcome | Persistent result |
| --- | --- |
| Success | Chronicle: Westfall defended; proof `world.westfall.defended`; Westfall campaign Complete; exactly one pending Success reward claim |
| Partial | Chronicle: Westfall bloodied; proof `world.westfall.bloodied`; campaign stays Active |
| Ignored | Chronicle: Westfall crisis ignored; proof `world.westfall.emergency`; one pressure-increased event; campaign stays Active |

Record the score and expected outcome in the acceptance record.

## 7. Capture the pre-restart snapshot

On the server host, from the FURY-Azeroth repository:

```bash
bash scripts/verify-t34-live-acceptance.sh snapshot fury-t34
```

This must pass before restart.

It creates:

```text
t34-fury-t34.snapshot
```

Do not perform more gameplay after this snapshot.

## 8. Restart worldserver

Stop worldserver normally and start it again using the same databases and
configuration.

Do not wipe, reseed or manually modify FURY/Living World state.

After startup, run:

```text
.fury validate
.fury status
.fury defias
```

Expected:

- validation remains HEALTHY;
- no orphan/duplicate Defias invasion appears;
- no new Director run is created merely because of restart.

## 9. Verify post-restart identity

Without doing additional combat/crafting/contract gameplay:

```bash
bash scripts/verify-t34-live-acceptance.sh post-restart fury-t34 t34-fury-t34.snapshot
```

The verifier requires all of the following to remain coherent across restart:

- same terminal Director run and outcome;
- same canonical resolved event id and revision;
- no duplicate active run;
- contract state unchanged;
- Field Relief Profession Order unchanged;
- Defias Bestiary checksum unchanged;
- exactly one outcome Chronicle entry;
- exactly one outcome proof;
- campaign status unchanged;
- Success reward claim count unchanged;
- Ignored pressure event count unchanged.

A pass from this command is the machine-readable evidence for the restart
portion of T34.

## 10. Record acceptance evidence

Copy `docs/T34_ACCEPTANCE_RECORD_TEMPLATE.md` to a dated acceptance record
and fill in:

- commit SHA;
- worldserver build/run reference;
- both Human account/character labels;
- Director run id;
- Living World runtime id;
- score and outcome;
- completed contract;
- Field Relief profession path;
- Bestiary entry progressed;
- pre-restart verifier PASS;
- post-restart verifier PASS;
- any recovery log lines.

T34 and M3 become GREEN only when:

1. the automated T34 GS10-GS18 CI job is green; and
2. one real two-Human acceptance record contains both verifier PASS results.
