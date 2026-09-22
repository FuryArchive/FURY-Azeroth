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

It runs automatically only when a pull request changes the integration
surface itself:

- the pinned upstream lock;
- module CMake/configuration;
- module loader/lifecycle entrypoints;
- the module-database lifecycle glue (`FuryDatabaseScript.cpp`);
- build, upstream-sync, DB lifecycle, runtime-data, or worldserver smoke scripts.

Ordinary domain changes do **not** auto-run Full Runtime. This includes domain
SQL/migrations, prepared statements, FuryApp composition, Campaign, Proofs,
Contracts, Director, Profession Orders, Bestiary, policies, projections, and
gameplay event handlers. Those changes are accepted through MySQL golden gates
and the fast `mod-fury` translation-unit compile unless a developer explicitly
starts Full Runtime.

It can also be started manually with **Run workflow** / `workflow_dispatch`.

## Milestone rule

Before closing a major milestone such as M2, run Full Runtime manually on the
final milestone head even if no path filter required it.

Do not add Full Runtime back to the default fast workflow. Add or adjust a
path trigger in `full-runtime.yml` when a new integration-sensitive subsystem
appears.
