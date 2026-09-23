# T31 — Adaptive Field Relief profession order

T31 completes the optional Field Relief contract through the generic T17
Profession Order engine. It does not add a separate crafting counter.

## Adaptive paths

The board selects the first supported profession the interacting Human
actually knows. Every selected recipe is starter-tier so the order is useful
for a low-level Westfall household instead of requiring late-game profession
progression.

Priority is deterministic:

| Ordinal | Skill | Skill id | Item | Item id | Required |
|---:|---|---:|---|---:|---:|
| 1 | First Aid | 129 | Linen Bandage | 1251 | 8 |
| 2 | Alchemy | 171 | Minor Healing Potion | 118 | 5 |
| 3 | Cooking | 185 | Charred Wolf Meat | 2679 | 8 |
| 4 | Leatherworking | 165 | Light Armor Kit | 2304 | 4 |
| 5 | Blacksmithing | 164 | Rough Sharpening Stone | 2862 | 6 |
| 6 | Tailoring | 197 | Bolt of Linen Cloth | 2996 | 6 |
| 7 | Engineering | 202 | Rough Blasting Powder | 4357 | 8 |

If the interacting character knows none of these professions, Available Field
Relief is omitted from that character's board view. It is still optional and
the Defias Success threshold remains reachable without its 10 score points.

An Active or Complete Field Relief contract is not hidden; only the initial
Available row is capability-filtered.

## Acceptance flow

When a Human selects Available Field Relief:

1. the board resolves the deterministic profession option;
2. it appends the normal durable contract-board request event;
3. ContractService accepts Field Relief against the current Director run;
4. only after the contract acceptance succeeds, ProfessionOrderService starts
   `classic.westfall.defias.field_relief.supplies` with that option ordinal.

The profession order is `Once` per household. T17's generic engine remains the
single authority for exact skill+item matching, per-unit craft progress,
last-event replay guards and crash-safe completion.

## Completion bridge

When the generic order emits its canonical
`profession.order.completed` event, Defias orchestration emits:

- event: `defias.field_relief.completed`;
- subject: `defias_contract_signal:1`;
- source: `fury.defias`;
- correlation: `classic.westfall.defias.field_relief`.

The dedupe identity is based on the profession-order instance id, so replaying
the order completion cannot create a second Field Relief signal. The T27
contract objective already consumes exactly this signal.

A signal created before a future contract acceptance cannot retroactively
complete it because the generic Contract matcher requires
`event.id >= accepted_event_id`.

## Pinned content audit

`scripts/audit-t31-field-relief-upstream.sh` runs after the exact upstream
revision is synced in Fast mod-fury compile CI. It checks:

- profession skill ids against pinned `SharedDefines.h`;
- all seven item id + name pairs against pinned
  `data/sql/base/db_world/item_template.sql`.

This is the final authority for T31 content ids; external databases are only
used during design, never as the CI source of truth.

## Validation

`scripts/test-t31-field-relief.sh` covers:

- deterministic profession selection and no-profession behavior;
- the existing T17 profession-order replay/schema gate;
- one enabled Once order with seven exact options;
- repeatable/idempotent T31 migration;
- the stable Field Relief completion signal;
- post-acceptance contract-objective matching;
- board-to-order and order-to-contract wiring.

Fast `mod-fury` compile validates the Player skill API, board integration and
Defias completion bridge against the pinned AzerothCore source.
