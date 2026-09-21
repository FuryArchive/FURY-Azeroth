# FURY Azeroth

Private long-lived cooperative AzerothCore project for two human players.

## Current implementation target

The repository is building toward:

```text
M1 — FURY Kernel
M2 — Campaign Platform
M3 — Defias Resurgence vertical slice
```

The implementation contract lives in `AGENTS.md`.
The executable backlog lives in `TASKS_M1_M3.md`.

## Reproducible upstream workspace

Third-party source is not vendored into this repository. Exact revisions are pinned in:

```text
vendor/lock/fury.lock.yaml
```

Resolve them with:

```bash
bash scripts/sync-upstreams.sh
```

This creates an ignored workspace at:

```text
upstream/azerothcore-wotlk/
```

with the pinned Playerbots AzerothCore fork and required external modules checked out at immutable commits.

## Gate 0 evidence

- `docs/REPOSITORY_INTAKE.md`
- `docs/UPSTREAM_API_NOTES.md`
- `docs/STATUS.md`

No milestone is considered GREEN until its build/test acceptance gate actually passes.
