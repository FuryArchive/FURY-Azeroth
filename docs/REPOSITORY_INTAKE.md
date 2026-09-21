# Repository Intake — T00.1

Date: 2026-09-22

## Canonical repository

- Repository: `FuryArchive/FURY-Azeroth`
- Visibility: private
- Default branch: `main`
- Bootstrap HEAD inspected: `8fd91a9a6a9af3de2ad441117a8ee29e8f295a0c`
- First lockfile commit: `e511114333bb51dcecfd36283c0ec01441898eab`

## State found during intake

The repository contains the FURY bootstrap/architecture files only. It does not yet vendor or check out AzerothCore or external modules.

Present:
- `AGENTS.md`
- `TASKS_M1_M3.md`
- `docs/STATUS.md`
- `README.md`
- `.gitignore`

Absent at intake:
- AzerothCore source tree
- `mod-playerbots` source
- `mod-individual-progression` source
- `mod-living-world` source
- `mod-fury` implementation
- build scripts
- database bootstrap scripts
- CI workflows
- upstream lockfile

The upstream lockfile was therefore created as part of Gate 0 instead of inferring revisions from a local checkout.

## Chosen workspace layout

FURY source stays in this repository. Third-party source is resolved into an ignored local workspace:

```text
FURY-Azeroth/
├── modules/
│   └── mod-fury/                  # first-party, tracked
├── upstream/                      # generated, ignored
│   └── azerothcore-wotlk/
│       └── modules/
│           ├── mod-playerbots/
│           ├── mod-individual-progression/
│           ├── mod-living-world/
│           └── mod-fury -> ../../../../modules/mod-fury
├── vendor/lock/fury.lock.yaml
└── scripts/sync-upstreams.sh
```

This avoids nested untracked Git repositories in the project while still producing a normal AzerothCore source tree for CMake.

## Upstream pins selected

See `vendor/lock/fury.lock.yaml`.

- Playerbots AzerothCore fork: `7f12e89ee5f467a50e62eba1d525eac7dc953d03`
- mod-playerbots: `7bae1b5c58c76a0aa20381155edc08096d1485b2`
- Individual Progression: `977e2005bacf97f35e506eb27b8af6b2ea1136af`
- Living World: `116926ef9ce42ee0bba223c1e406cf62cedd9904`

These are immutable implementation pins, not a claim that the combination has already passed a FURY build. Compatibility remains `PINNED_NOT_YET_BUILD_VERIFIED` until the first clean build.

## Build entrypoint

The resolved build root will be:

```text
upstream/azerothcore-wotlk/
```

The project will use the normal AzerothCore CMake build from that tree with external modules placed under its `modules/` directory.

A FURY wrapper build script will be added before T01 can be marked GREEN.

## Database bootstrap path

Core databases remain owned by AzerothCore / Playerbots.

FURY will add its own module-owned database in T02 using the verified `DatabaseScript::OnModuleDatabasesLoading()` lifecycle. The Playerbots module at the selected pin provides a concrete working implementation pattern using `ModuleDBUpdater`.

## Intake result

T00.1 repository inspection is complete.

What is verified:
- canonical repository and branch;
- actual bootstrap HEAD;
- absence of pre-existing implementation/source that could be overwritten;
- intended upstream workspace layout;
- exact upstream repositories selected for locking.

What is not yet claimed:
- successful compilation;
- successful DB bootstrap;
- external-module compatibility under the selected pins.

Those belong to T00.2/T00.3 and the M1 gate.
