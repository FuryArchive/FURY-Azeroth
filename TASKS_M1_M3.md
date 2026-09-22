# FURY Azeroth — Executable Backlog M1–M3

Current execution status is tracked in this file and `docs/STATUS.md`. Do not infer GREEN from source presence alone; compilation/runtime acceptance remains mandatory.

## Gate 0 — Repository intake

### T00.1 Inspect repository
Status: GREEN

Actions:
- record branch, HEAD, remote and `git status`;
- locate AzerothCore root and module layout;
- locate/verify `mod-playerbots`, `mod-individual-progression`, and `mod-living-world`;
- locate existing build/test scripts and database config;
- inspect current lock/pinning mechanism.

Acceptance:
- no user changes overwritten;
- exact upstream commits/branches recorded;
- build entrypoint identified;
- DB bootstrap path identified.

### T00.2 Verify pinned upstream APIs
Status: GREEN
Depends on: T00.1

Verify in the actual checkout:
- Playerbots real-player detection API;
- `DatabaseScript` module database lifecycle;
- PlayerScript hook signatures used by M1/M2;
- module loader naming convention;
- Living World public/private runtime APIs and exact Defias IDs/signals.

Acceptance:
- `docs/UPSTREAM_API_NOTES.md` contains file paths + commit hashes + signatures;
- every later task references only verified APIs.

### T00.3 Establish lock file
Status: GREEN
Depends on: T00.1

Create/update `fury.lock.yaml` with exact commits for core and required modules.

Acceptance:
- clean checkout can resolve the same versions;
- no `latest`/floating branch is used for production reproducibility except as descriptive metadata.

---

# M1 — FURY Kernel

## T01 Module skeleton
Status: GREEN
Depends on: T00.2
Commit target: `chore: add mod-fury skeleton`

Create `modules/mod-fury` using the current AzerothCore module loader conventions.

Minimum files:
- module loader;
- config `.conf.dist`;
- `src/core/FuryApp.*` placeholder;
- module README/version identifier.

Acceptance:
- AzerothCore configures with module enabled;
- worldserver links;
- worldserver starts with FURY enabled/disabled;
- no gameplay behavior yet.

## T02 Module-owned database
Status: GREEN
Depends on: T01
Commit target: `feat(db): add module-owned acore_fury database`

Implement:
- `FuryDatabasePool`;
- FURY MySQL connection type;
- `FuryDatabaseScript`;
- create/populate/update lifecycle;
- DB revision reporting;
- connection config.

Initial schema contains only schema metadata if needed.

Acceptance:
- empty MySQL instance -> `acore_fury` initialized on worldserver startup;
- second startup performs no destructive reinitialization;
- bad connection/schema migration fails startup clearly;
- module revision is visible in diagnostics/logs.

## T03 FuryApp composition root
Status: GREEN
Depends on: T01, T02
Commit target: `feat(core): add FuryApp composition root`

Implement lifecycle:
- Initialize;
- Update with throttled internal timers;
- Shutdown.

Acceptance:
- no expensive DB scan per world tick;
- services initialize/shutdown deterministically;
- startup failure leaves a clear error.

## T04 ActorContext + actor resolver
Status: GREEN
Depends on: T00.2, T03
Commit target: `feat(actor): add actor classification`

Implement actor kinds:
- Human;
- HouseholdAltBot;
- RandomPlayerBot;
- System.

First implementation may classify HouseholdAltBot after Household service exists; structure API now to support it.

Acceptance:
- real player -> Human;
- random bot -> RandomPlayerBot;
- null/system event -> System;
- tests do not depend on account-name heuristics.

## T05 Household schema/repository/service
Status: GREEN
Depends on: T02, T04
Commit target: `feat(household): add household repository and service`

Implement:
- `fury_household`;
- `fury_household_member`;
- create household;
- add/remove member;
- lookup by account;
- M1 limit: two human member accounts.

Acceptance:
- account belongs to at most one household;
- duplicate add is idempotent or returns a stable conflict result;
- HouseholdAltBot classification works for a bot character whose account belongs to the household;
- random bots remain RandomPlayerBot.

## T06 Durable Event Store
Status: GREEN
Depends on: T02, T04, T05
Commit target: `feat(events): add durable Fury event store`

Implement normalized `FuryEvent` and `fury_event` table.

Required fields:
- event type;
- actor kind/guid/account/household;
- map/zone/area;
- subject type/id;
- source system;
- correlation key;
- stable dedupe key;
- JSON payload;
- timestamp.

Acceptance:
- append returns durable event id;
- duplicate dedupe key creates no second event;
- indexes support household/type/subject scans;
- dedupe key is based on authoritative identity, never wall-clock time alone.

## T07 Consumer checkpoint/replay
Status: GREEN
Depends on: T06
Commit target: `feat(events): add consumer checkpoints and replay`

Implement `fury_event_consumer` and replay batches.

Acceptance:
- consumer resumes after last committed event id;
- crash after event append but before consumer checkpoint replays event;
- replayed domain mutation can be made idempotent;
- batch size configurable.

## T08 AzerothCore event collector
Status: GREEN
Depends on: T00.2, T06
Commit target: `feat(events): add PlayerScript event collector`

Initial hooks:
- login;
- level change;
- zone/area change;
- quest complete;
- creature kill;
- creature killed by pet;
- item create/store where semantically appropriate.

Acceptance:
- hook code only normalizes/publishes events;
- no Contract/Reward/Chronicle mutations in hook methods;
- duplicate authoritative callbacks dedupe correctly where a stable identity exists.

## T09 Reward claim kernel
Status: GREEN
Depends on: T02, T06
Commit target: `feat(rewards): add reward claim registry`

Implement:
- reward bundle definition;
- reward entries;
- claim uniqueness by source + reward + beneficiary;
- status lifecycle;
- policy interface with power-band check placeholder.

M1 does not need valuable physical item delivery.

Acceptance:
- replayed source event cannot create a second claim;
- denied policy returns stable reason;
- pending claims can be reconciled.

## T10 Chronicle projection
Status: GREEN
Depends on: T06, T07
Commit target: `feat(chronicle): add Chronicle projection`

Implement player-facing history projection from durable events.

Acceptance:
- Chronicle is not used as the machine event source;
- duplicate event replay does not duplicate an entry;
- household timeline query works in deterministic order.

## T11 Diagnostics and validation shell
Status: GREEN
Depends on: T03–T10
Commit target: `feat(commands): add .fury diagnostics`

Minimum commands:
- `.fury status`;
- `.fury actor`;
- `.fury household status`;
- `.fury event tail`;
- `.fury reward claims`;
- `.fury validate`.

Acceptance:
- commands are permission-gated;
- diagnostics do not mutate state unless command explicitly says so;
- validation prints actionable subsystem/key/error information.

## T12 M1 automated gate
Status: GREEN
Depends on: T01–T11
Commit target: `test: add M1 kernel golden scenarios`

Mandatory scenarios:
- GS01 event dedupe;
- GS02 real human actor;
- GS03 random bot actor;
- GS04 household alt-bot actor;
- event replay after simulated consumer interruption;
- reward claim idempotency;
- Chronicle projection replay;
- clean DB startup + second startup.

M1 is GREEN only when all mandatory scenarios, clean module build, and worldserver startup smoke pass.

---

# M2 — Campaign Platform

## T13 Campaign schema/service
Status: GREEN
Depends on: M1 GREEN
Commit target: `feat(campaign): add household campaign state`

Implement definition + runtime state and explicit transition rules.

Acceptance:
- invalid backward transitions rejected;
- duplicate completion idempotent;
- household current power band derives from canonical campaign progression, not arbitrary module state.

## T14 Proof service
Status: GREEN
Depends on: T13
Commit target: `feat(proofs): add durable household proofs`

Acceptance:
- proof key unique per household;
- duplicate grant idempotent;
- source event retained;
- proof lookup is cheap and indexed.

## T15 Contracts core
Status: GREEN
Depends on: T07, T09, T13
Commit target: `feat(contracts): add contract definitions and runtime`

Implement:
- definitions;
- objectives;
- household instances;
- objective progress;
- event consumer;
- completion event.

Acceptance:
- replayed kill/craft event increments once;
- unrelated events do not scan every objective naively;
- completion emits one durable event;
- bot eligibility policy is enforced centrally.

## T16 Director runtime
Status: GREEN
Depends on: T07, T13, T14
Commit target: `feat(director): add persistent Director runtime`

Implement runs, phases, outcomes, revision checks, reconciliation interface.

Acceptance:
- one exclusive active graph per configured scope;
- duplicate start request cannot create two active runs;
- optimistic revision mismatch reloads/re-evaluates;
- Director state survives restart.

## T17 Minimal profession-order subsystem
Status: GREEN
Depends on: T08, T15
Commit target: `feat(professions): add profession order tracking`

Implement generic craft-event objective tracking. Do not add profession XP.

Acceptance:
- crafted-item event can progress matching active order;
- wrong profession/item does not progress;
- replay does not double-count.

## T18 Minimal Bestiary projection
Status: GREEN
Depends on: T08
Commit target: `feat(bestiary): add minimal discovery projection`

Implement account-level entries with Unknown/Encountered/Studied/Mastered.

Acceptance:
- normal creature events can be filtered by explicit content mapping;
- replay-safe progression;
- no Hunt engine behavior in M2.

## T19 Individual Progression read-only adapter
Status: GREEN
Depends on: T00.2, T13
Commit target: `feat(ip): add read-only Individual Progression adapter`

Acceptance:
- no UPDATE/DELETE to IP-owned tables/state;
- adapter reports unavailable cleanly if module absent/incompatible;
- campaign eligibility combines household unlock + character gate at service boundary.

## T20 M2 automated gate
Status: GREEN
Depends on: T13–T19
Commit target: `test: add campaign platform golden suite`

Mandatory tests:
- campaign transitions;
- household/character gate separation;
- proof idempotency;
- contract replay idempotency;
- Director duplicate-start protection;
- persistence across restart/repository reload;
- Bestiary/profession event filtering.

M2 is GREEN only with M1 regression gate also green.

---

# M3 — Defias Resurgence Vertical Slice

## T21 Living World source audit
Status: GREEN
Depends on: M2 GREEN, T00.2
Commit target: documentation only unless mismatch found

Inspect exact pinned `mod-living-world`:
- Defias invasion id/key;
- stage ids;
- runtime signal ids;
- spawn group ids;
- runtime persistence semantics;
- existing public API boundaries.

Acceptance:
- all identifiers captured from source/SQL, not copied from an old design document;
- any mismatch updates the FURY content mapping before code changes.

## T22 Living World external bridge patch
Status: GREEN
Depends on: T21
Patch queue target: `vendor/patches/mod-living-world/0001-fury-bridge.patch`

Expose only what FURY still requires after T21:
- stable reverse runtime entity metadata lookup by creature GUID;
- compatibility guard for the pinned API/structure.

Do not patch APIs that T21 confirmed are already public: scheduler controlled start, runtime/state query, signal send, authored-data query. Use polling/reconciliation instead of adding lifecycle/stage callbacks.

Acceptance:
- bridge contains no FURY Household/Contracts/Reward concepts;
- upstream module still works without `mod-fury` loaded;
- bridge patch applies cleanly to pinned commit;
- a compatibility guard fails clearly after incompatible upstream changes.

## T23 FURY LivingWorldAdapter
Status: GREEN
Depends on: T22
Commit target: `feat(lw): add Living World adapter`

Acceptance:
- external callbacks become normalized FuryEvents;
- Director calls adapter, never LW internals directly;
- adapter can query current runtime during restart reconciliation.

## T24 Defias content validation + disable random start
Status: READY
Depends on: T21, T23
Commit target: `content(defias): pin FURY-controlled invasion`

Acceptance:
- FURY validates required invasion/stages/signals/spawn groups at startup;
- random start disabled through an explicit FURY-owned overlay/migration;
- missing required definition gives actionable validation failure;
- no Director run starts against invalid content.

## T25 Defias Director graph
Status: READY
Depends on: T16, T23, T24
Commit target: `feat(defias): add Director graph`

Phases:
- Rumours;
- Invasion;
- FinalBattle;
- Resolution;
- Success/Partial/Ignored outcome.

Acceptance:
- first eligible human Westfall trigger creates at most one run;
- no bot-only trigger;
- run survives restart;
- LW start occurs only after configured activation condition.

## T26 Westfall contract board
Status: READY
Depends on: T15, T25
Commit target: `feat(defias): add Westfall contract board`

Server-side gossip/GameObject only; no mandatory addon.

Acceptance:
- only contextually available contracts shown;
- active/completed status visible;
- no custom client patch required.

## T27 Six Defias contracts
Status: READY
Depends on: T21, T26
Commit target: `feat(defias): add initial Westfall contracts`

Contracts:
- Recon Roads;
- Break Scouts;
- Break Control;
- Hold Sentinel;
- Field Relief;
- Defeat Commander.

Acceptance:
- ordinary non-runtime Westfall Defias kills do not count;
- runtime group mapping uses verified LW metadata;
- bot-only actions do not progress persistent FURY objectives;
- replay-safe.

## T28 Human participation resolver
Status: READY
Depends on: T04, T21
Commit target: `feat(defias): add human participation tracking`

Acceptance:
- pet kill can credit the human owner;
- bot final blow does not erase legitimate nearby human encounter participation;
- random nearby non-participants do not receive credit;
- radius/time/group rules are configurable/tested.

## T29 Participation score/outcome policy
Status: READY
Depends on: T27, T28
Commit target: `feat(defias): add participation scoring`

Initial score model totals 100 and thresholds default to Success >=70, Partial >=30.

Acceptance:
- score components data/config driven;
- repeated completion event cannot award a component twice;
- outcome is deterministic for recorded component set.

## T30 Defias Bestiary content
Status: READY
Depends on: T18, T27
Commit target: `content(defias): add Bestiary entries`

Acceptance:
- only explicitly mapped runtime/content targets advance Defias entries;
- commander + successful outcome can produce Mastered under defined rules;
- no mod-hunts dependency yet.

## T31 Adaptive Field Relief profession order
Status: READY
Depends on: T17, T27
Commit target: `content(defias): add Field Relief order`

Before final SQL, verify item/skill IDs in the actual pinned world DB.

Acceptance:
- at least one valid option for each selected supported profession path;
- no suitable profession -> contract remains optional and cannot softlock campaign;
- craft events replay safely.

## T32 Defias persistent resolution
Status: READY
Depends on: T14, T29
Commit target: `feat(defias): add persistent outcomes`

Success:
- Chronicle success;
- `world.westfall.defended` proof;
- campaign completion;
- configured FURY rewards.

Partial:
- partial Chronicle;
- `world.westfall.bloodied` proof;
- campaign continues.

Ignored:
- ignored Chronicle;
- pressure increase/emergency content flag;
- no vanilla quest softlock.

Acceptance:
- outcome replay idempotent;
- campaign cannot be permanently blocked by Partial/Ignored;
- valuable physical reward duplication is impossible or valuable physical rewards are withheld until a safe receipt mechanism exists.

## T33 Director/Living World reconciler
Status: READY
Depends on: T16, T23, T25
Commit target: `feat(recovery): reconcile Director and Living World`

Handle:
- Director active + LW active;
- Director active + LW absent;
- Director complete + orphan LW active;
- FURY-owned LW runtime + missing Director run.

Acceptance:
- restart mid-invasion does not create a second invasion;
- every mismatch has deterministic recovery or safe abort behavior;
- reconciliation is observable in logs/diagnostics.

## T34 Defias golden suite + acceptance runbook
Status: READY
Depends on: T24–T33
Commit targets:
- `test(defias): add vertical-slice golden suite`
- `docs: add Defias operational runbook`

Automated mandatory scenarios:
- GS10 underlevel/no eligibility;
- GS11 one valid start;
- GS12 duplicate simultaneous trigger -> one run;
- GS13 Success;
- GS14 Partial;
- GS15 Ignored;
- GS16 event append + consumer crash replay;
- GS17 restart during LW run;
- GS18 missing dependency/definition.

Manual/real-worldserver acceptance:
1. two human accounts form household;
2. eligible human enters Westfall;
3. Director creates Rumours;
4. contract board works;
5. LW invasion starts under FURY control;
6. humans + bots fight;
7. runtime targets, bot actions and human participation classify correctly;
8. at least one contract, Bestiary progression and profession order complete;
9. LW final stage completes;
10. Director resolves the expected outcome;
11. Chronicle/proof/campaign state persists;
12. restart worldserver;
13. state remains coherent and no reward/progress duplicates appear.

M3 is GREEN only after both automated gate and one documented vertical-slice acceptance run pass.

---

# Deferred backlog after M3

Do not silently pull these into current scope:

- M4 `mod-hunts` adapter and reward audit;
- richer Bestiary/trophy system;
- full profession discoveries/masterworks;
- Base physical progression;
- Relics;
- collections integration;
- Gold Rush audit;
- Dungeon Master expeditions;
- Classic Silithid and Scourge Director graphs;
- Era II/III verticalisation;
- Era IV Terror/Paragon/Infusion/Mythic+ framework.

---

# Immediate execution state

Gate 0 and **M1 FURY Kernel are GREEN**. PR #7 passed fast module compilation, MySQL 8 schema/idempotency, actor-policy golden scenarios, the pinned production worldserver build, real module database create/populate/update/restart, and two consecutive full worldserver startup/validation/shutdown smokes with pinned runtime data.

T13–T20 are GREEN and **M2 Campaign Platform is GREEN**. GitHub Actions run #118 passed the dedicated T20 aggregate gate, all M1 regressions, T13–T19 policy/schema scenarios, and Fast mod-fury compile.

Continue with **T21 Living World source audit** using only the exact pinned upstream revision before any M3 bridge/content code.
