# FURY CI tiers

FURY uses two CI tiers.

## Fast CI

`.github/workflows/ci.yml` runs on every pull request to `main` and on every push to `main`.

It contains the normal development gates:

- actor/policy golden tests;
- schema and replay golden tests;
- fast `mod-fury` compile against the locked upstream source.

New commits cancel obsolete in-progress fast runs for the same ref.

Fast CI is the default acceptance path for domain changes such as Contracts,
Bestiary, Director rules, projections, policies, and similar module logic.

## Full runtime CI

`.github/workflows/full-runtime.yml` performs the expensive locked AzerothCore
worldserver build, real FURY database lifecycle smoke, runtime-data install,
and two worldserver startup/runtime smokes.

It is intentionally **not** run for every pull request.

It runs automatically only when a pull request touches integration-sensitive
paths such as:

- the pinned upstream lock;
- module build/configuration;
- SQL/database integration;
- FuryApp/module-loader composition;
- event/household/diagnostics infrastructure used by startup/runtime smoke;
- AzerothCore player/command scripts;
- build, upstream-sync, DB lifecycle, runtime-data, or worldserver smoke scripts.

It can also be started manually with **Run workflow** / `workflow_dispatch`.

## Milestone rule

Before closing a major milestone such as M2, run Full Runtime manually on the
final milestone head even if no path filter required it.

Do not add Full Runtime back to the default fast workflow. Add or adjust a
path trigger in `full-runtime.yml` when a new integration-sensitive subsystem
appears.
