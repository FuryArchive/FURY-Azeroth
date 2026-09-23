# T34 — Defias vertical-slice acceptance runbook

T34 is the final M3 acceptance gate.

It has two independent parts:

1. the automated GS10–GS18 suite;
2. one documented live run on a real worldserver with two real human accounts.

M3 is not marked GREEN until both parts pass.

## Automated gate

Run:

```bash
bash scripts/test-t34-defias-vertical-slice.sh
```

The suite composes already-established subsystem gates rather than creating a
second implementation of the same rules.

| Scenario | Required behavior | Backing gate |
| --- | --- | --- |
| GS10 | underlevel / ineligible human cannot start Defias | T25 graph policy |
| GS11 | one eligible human trigger creates one run | T25 graph/schema |
| GS12 | duplicate simultaneous triggers still create one active run | T25 schema |
| GS13 | Success persists the success outcome | T32 resolution |
| GS14 | Partial persists and campaign can continue | T32 resolution |
| GS15 | Ignored persists pressure/emergency state and campaign can continue | T32 resolution |
| GS16 | event append / consumer crash replays safely | M1 EventStore + Director replay |
| GS17 | restart/runtime drift recovers without duplicate invasion | T33 recovery |
| GS18 | missing Living World/content definition fails safely | T24 content validation |

The T34 PR also triggers the existing full pinned worldserver runtime workflow,
which builds the real Playerbots AzerothCore stack, runs the FURY database
lifecycle and starts worldserver twice.

## Live acceptance prerequisites

Use the actual candidate server/client package that is intended for play.

Required server state:

- FURY enabled;
- Defias enabled;
- Living World enabled and Defias invasion 1 present;
- two human accounts available;
- both accounts are GM for the acceptance run, or one GM can perform household
  setup and diagnostics;
- no previous production household progress that matters. Use fresh test
  accounts/characters for this acceptance;
- characters at level 10 or above and able to enter Westfall;
- at least one test character has Alchemy, First Aid or Cooking so Field Relief
  can be exercised;
- keep one Playerbot/household-alt available for mixed human+bot combat.

Before starting, run:

```text
.fury status
.fury validate
.fury defias
```

Acceptance cannot begin if `.fury validate` reports FAILED or Defias reports
`enabled=no`, `content_valid=no` or `scoring=no`.

## Live run

### 1. Create the household

Log in on human account A and run:

```text
.fury household create t34-live
```

Select the character from human account B and run:

```text
.fury household add
.fury household status
```

Expected:

- both real accounts resolve as Human;
- household reports exactly 2/2 members;
- neither account is treated as a Playerbot.

Useful check on both humans:

```text
.fury actor
```

### 2. Enter Westfall

Move an eligible human into Westfall.

Expected:

- one Defias Director run appears;
- phase starts at `rumours`;
- repeated zone/login/level events do not create a second active run.

Check:

```text
.fury defias
.fury event tail 20
```

### 3. Use the contract board

Open the FURY Westfall contract board.

Expected:

- the six Defias contracts are visible with correct status;
- accepting a contract records one durable acceptance;
- replay/reopening does not duplicate acceptance/progress;
- Field Relief is optional;
- a character with Alchemy, First Aid or Cooking can accept Field Relief and
  receives the matching profession-order target;
- a character without those skills is not softlocked.

### 4. Activate the invasion

Trigger the configured activation path (contract or presence).

Expected:

- Director phase becomes `invasion`;
- exactly one Living World runtime is attached;
- `.fury defias` prints a non-zero runtime id;
- no second invasion is created by another activation event.

### 5. Fight with humans and bots

Use both human players and at least one bot during the invasion.

Expected:

- physical Living World enemies/stages behave normally;
- bot participation does not author persistent Human progression by itself;
- actual Human participation produces the mapped Defias progress;
- score changes only from defined T29 components.

Check periodically:

```text
.fury defias
.fury event tail 30
```

### 6. Exercise Contracts, Bestiary and Profession Order

During the same run:

- complete at least one ordinary Defias contract;
- kill mapped Defias runtime targets with a Human participant;
- progress at least one Defias Bestiary entry;
- complete Field Relief through a real craft event.

Expected:

- contract progress reaches completion once;
- Bestiary progresses only through mapped FURY-owned runtime entities;
- Field Relief completes through
  `profession.order.completed -> defias.field_relief.completed`;
- bot-only actions cannot satisfy those permanent Human requirements.

### 7. Complete the Living World final stage

Finish the final Defias stage.

Expected:

- Director reaches `resolution`;
- T29 score selects Success, Partial or Ignored according to the configured
  thresholds;
- one canonical `director.run.resolved` event exists;
- the run row points `resolved_event_id` at that event.

For the official T34 live acceptance, **Success is preferred**, because it also
exercises campaign completion and the reward entitlement. Partial/Ignored may
be tested separately without invalidating the runbook.

### 8. Verify persistent outcome

For Success, expected:

- Chronicle contains **Westfall defended**;
- household has proof `world.westfall.defended`;
- `campaign.classic.westfall` is Complete;
- exactly one pending reward claim exists for
  `classic.westfall.defias.success`;
- no valuable physical reward has been duplicated or auto-materialized.

Check:

```text
.fury event tail 50
.fury reward claims 20
.fury validate
```

### 9. Restart worldserver mid-state test

The live acceptance must include one actual restart test.

The preferred sequence is:

1. perform a fresh Partial/Ignored test run or use a disposable household;
2. stop worldserver while the Defias Living World invasion is active;
3. start worldserver again;
4. allow at least one reconcile interval;
5. inspect `.fury defias` and `.fury event tail 50`.

Expected:

- the existing runtime is reattached if Living World restored it;
- if the bound runtime is genuinely absent, exactly one replacement starts;
- if a replacement was created before the previous crash completed its FURY
  binding, it is adopted rather than creating another runtime;
- orphan Living World runtime with no active Director is failed/cleaned;
- recovery is visible through `director.recovery.*` and/or
  `director.runtime.recovered`;
- no duplicate contract, Bestiary, score, proof, Chronicle or reward state is
  produced.

### 10. Final validation

Run:

```text
.fury status
.fury validate
.fury defias
.fury event tail 50
.fury reward claims 20
```

Acceptance passes only if:

- validation is HEALTHY;
- no duplicate active Director run exists;
- no orphan Defias Living World runtime remains;
- all expected persistent state survived restart;
- all reward/progress projections remain single-instance;
- both human accounts remain in the same household.

## Acceptance record

When the live run is performed, record the result in
`docs/acceptance/T34_LIVE_RESULT.md` using this minimum evidence:

```text
Date:
Build/commit:
Client package:
Human account A character:
Human account B character:
Household id:
Director run id:
Living World runtime id before restart:
Living World runtime id after restart:
Outcome:
Score:
Completed contract:
Bestiary entry exercised:
Field Relief profession/path:
Proof observed:
Campaign status:
Reward claim id/status:
Recovery event(s):
.fury validate before: HEALTHY/FAILED
.fury validate after: HEALTHY/FAILED
Result: PASS/FAIL
Notes:
```

Do not record account passwords or other credentials.

## M3 completion rule

T34 automated GS10–GS18 green + pinned full-worldserver smoke green means
**READY FOR LIVE ACCEPTANCE**.

Only a documented live PASS with two humans changes M3 itself to **GREEN**.
