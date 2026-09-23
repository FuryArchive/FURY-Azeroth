# FURY Azeroth

Private long-lived cooperative AzerothCore project for two human players.

## Current implementation target

The repository has accepted:

```text
M1 — FURY Kernel
M2 — Campaign Platform
```

M3 — Defias Resurgence is still being completed, but module-stack completion now comes first so the vertical slice is accepted against the actual long-term game stack rather than a temporary minimal server.

The implementation contract lives in `AGENTS.md`.
The executable backlog lives in `TASKS_M1_M3.md`.
The selected external module stack lives in `docs/MODULE_STACK.md`.

## Reproducible upstream workspace

Third-party source is not vendored into this repository. Exact revisions are pinned in:

```text
vendor/lock/fury.lock.yaml
```

The sync script has three profiles.

Baseline — already accepted FURY dependencies:

```bash
bash scripts/sync-upstreams.sh
```

Selected server module stack:

```bash
FURY_STACK_PROFILE=server bash scripts/sync-upstreams.sh
```

Full compatibility workspace, including client/content integrations:

```bash
FURY_STACK_PROFILE=all bash scripts/sync-upstreams.sh
```

The generated workspace lives under:

```text
upstream/azerothcore-wotlk/
upstream/integrations/
```

The selected stack includes Playerbots, Individual Progression, Living World, AutoBalance, AHBot, staged progression tooling, zone difficulty, RDF expansion, dungeon-clear automation, AoE loot, account achievements, War Effort, Dungeon Master, Warband Camp, Nemesis, challenge modes, Quest Radar and LLM Chatter.

Client/content compatibility tracks additionally include SoloCollectionsPlatform, Worgen/Goblin, Delves and MythicPlus Extended with its standard-Eluna/AIO requirements.

See `docs/MODULE_STACK.md` for authority boundaries and activation order.

## Gate 0 evidence

- `docs/REPOSITORY_INTAKE.md`
- `docs/UPSTREAM_API_NOTES.md`
- `docs/STATUS.md`

No milestone or third-party compatibility target is considered GREEN until its required build/test acceptance gate actually passes.
