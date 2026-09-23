# T26 — Westfall contract board

T26 adds a first-party, server-side contract board for the Defias Resurgence
vertical slice. It uses AzerothCore's normal GameObject gossip surface: no
addon, custom client patch, or client-side UI protocol is required.

## Runtime behavior

The FURY-owned board is spawned at Sentinel Hill and bound to
`fury_westfall_contract_board`. The GameObject uses an existing 3.3.5 client
display asset and opens a normal gossip menu.

The board resolves the interacting player through the existing ActorResolver.
Only a real Human account in a FURY household can accept persistent contracts.
The board derives its context from the household's currently active
`classic.westfall.defias_resurgence.v1` Director run:

- campaign node: `campaign.classic.westfall`;
- Director run id;
- current Director phase.

Contracts are shown only when they belong to
`classic.westfall.contracts` and are contextually valid. A contract already
Active or Complete stays visible even after the Director phase moves on.

Selecting an Available row first appends a durable, deduplicated
`contract.board.accept.requested` FuryEvent and then calls the existing
ContractService acceptance path. The board never writes contract state
directly.

T27 supplies the six actual Defias contract definitions/objectives. Until then,
the board is intentionally empty in a fresh database.

## World data

FURY reserves template/spawn id `9000100` and creates one GOOBER in Westfall,
Sentinel Hill. The overlay is idempotent and uses an existing client display id,
so no client files are introduced.

## Validation

`scripts/test-t26-contract-board.sh` covers contextual availability policy,
active/completed visibility, disabled contract filtering, the server-side
GameObject gossip surface, absence of an addon dependency, and repeatable
MySQL world-overlay application without touching unrelated rows.

Fast `mod-fury` compilation is the API-level guard against the exact pinned
AzerothCore GameObjectScript/gossip surface.
