# T31 — Adaptive Field Relief profession order

T31 turns the optional **Field Relief** Defias contract into real crafting
gameplay without making a particular profession mandatory.

## Supported paths

The order key is classic.westfall.defias.field_relief and has three options:

| Path | Skill id | Crafted item | Item id | Required |
| --- | ---: | --- | ---: | ---: |
| Alchemy | 171 | Minor Healing Potion | 118 | 3 |
| First Aid | 129 | Linen Bandage | 1251 | 6 |
| Cooking | 185 | Roasted Boar Meat | 2681 | 6 |

These are baseline low-rank WotLK profession recipes. The existing T17 craft
collector resolves the profession skill from the actual recipe spell and emits
one durable profession.crafted event per physical crafted item.

## Adaptive selection

When a human selects Field Relief on the Westfall board, FURY reads that
character's current Alchemy, First Aid and Cooking skill values. The supported
path with the highest current skill is selected. Ties are deterministic.

If the character has none of the supported skills, the board shows Field Relief
as optional and profession-gated instead of accepting an impossible contract.
Field Relief completion is not required to advance the Defias campaign.

## Completion bridge

T17 remains the generic Profession Order engine. T31 adds a Defias projection:

profession.order.completed -> defias.field_relief.completed

The Defias event uses subject_type defias_contract_signal, subject_id 1, and
correlation_key classic.westfall.defias.field_relief.

That is exactly the stable objective authored by T27. The event identity is
based on the Profession Order instance id, so completion replay is idempotent.
The Contract engine's accepted-event cutoff prevents an older order completion
from satisfying a later Director-run Field Relief contract.

## Authority

Only Human craft events can progress Profession Orders. Household alt-bots and
random population bots remain unable to author persistent FURY profession
progress.

## Validation

scripts/test-t31-field-relief.sh validates adaptive option selection,
no-profession behavior, exact content targets, stable signal matching,
replay-safe progress, idempotent SQL, board-to-order wiring and
order-completion-to-Defias-signal wiring.
