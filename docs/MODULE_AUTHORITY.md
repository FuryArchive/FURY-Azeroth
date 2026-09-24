# FURY Azeroth — Module Authority Matrix

Date: 2026-09-24

This document defines who is allowed to own each persistent gameplay decision in the selected third-party stack. A module may execute gameplay without becoming the authority for campaign state, durable household credit or long-term rewards.

## Global rules

FURY owns:

- household identity and Human-vs-bot credit;
- campaign phase and canonical story progression;
- Chronicle and Proof records;
- durable cross-system reward claims;
- Director outcomes and world-state consequences;
- the canonical Mythic+ identity once MythicPlus Extended is integrated.

Third-party modules may own their local runtime state, UI and moment-to-moment gameplay. They must not silently create a second authority for the categories above.

## Server stack

| Module | Allowed authority | FURY boundary |
| --- | --- | --- |
| Playerbots | bot AI, group behavior, bot execution | bots never author Human-only household credit |
| Individual Progression | per-character historical-content restrictions | does not choose household campaign phase |
| Living World | physical invasion/event execution | Director owns canonical run/outcome/persistence |
| AutoBalance | instance stat scaling for small groups | experimental boss-token reward system stays OFF |
| AHBot | auction-house market simulation | no campaign/reward authority |
| Progression System | applies a selected Classic/TBC/WotLK content bracket | all brackets ship OFF; FURY/operator chooses the active bracket |
| Zone Difficulty | optional zone/instance difficulty controls | built-in Mythic mode stays OFF; not canonical Mythic+ |
| RDF Expansion | dungeon-finder availability across eras | no campaign authority |
| Dungeon Clear | Playerbot routing, pulls and encounter execution | no durable reward/progression authority |
| AoE Loot | loot interaction QoL | does not create new loot/reward tables |
| Account Achievements | account-wide achievement projection | owns achievement sharing only |
| War Effort | AQ event mechanics and its native quest flow | FURY campaign may observe/wrap completion; native quest-required items remain intact |
| Dungeon Master | procedural/roguelike dungeon runtime | generated loot, custom kill XP, completion gold/items and roguelike rewards are disabled until routed through FURY |
| Warband Camp | account camp/housing, mailbox, training, rested-area convenience | no campaign/reward authority; future FURY Camping may build on it |
| Nemesis System | nemesis creation, rank, affixes, revenge/bounty runtime | direct bounty/revenge gold/items are disabled until routed through FURY |
| Challenge Modes | voluntary per-character challenge constraints and XP modifiers | direct title/talent/item/achievement reward lists remain empty |
| Quest Radar | quest objective QoL | no progression authority |
| LLM Chatter | cosmetic/social bot dialogue and bot memory | must never author gameplay credit, rewards or campaign outcomes |

## Executable enforcement

`scripts/test-selected-module-authority.sh` validates the current synced server profile.

The gate currently enforces:

- Zone Difficulty Mythic mode OFF;
- AutoBalance experimental direct rewards OFF;
- Dungeon Master direct persistent rewards OFF;
- Nemesis direct persistent rewards OFF;
- no Progression System bracket enabled by default;
- no direct Challenge Modes title/talent/item/achievement reward list enabled by default.

`scripts/sync-upstreams.sh` runs this gate automatically for the `server` and `all` profiles after vendor patches are applied.

## Deferred adapters

Dungeon Master and Nemesis keep their gameplay loops active, but their durable reward surfaces are intentionally suppressed for now. The later FURY adapters should consume explicit completion/kill events and mint Reward Registry claims instead of re-enabling the upstream direct grants.

MythicPlus Extended remains deferred until the standard-Eluna + Playerbots core reconciliation is complete. When activated, Zone Difficulty Mythic and any Dungeon Master "Mythic+ style" identity remain secondary mechanics and must not become a competing canonical keystone/rating system.
