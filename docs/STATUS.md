# FURY Azeroth — Implementation Status

Date: 2026-09-22

## Current state

- Product/content architecture: defined through v0.2/v0.3 design passes.
- Implementation contract: READY.
- M1–M3 executable backlog: READY.
- Live repository intake: BLOCKED — repository was not present in this working session.
- Source modifications: none claimed.
- Builds/tests: none claimed.

## Verified design assumptions from current public upstream

- `mod-playerbots` requires the Playerbots AzerothCore fork rather than stock AzerothCore.
- AzerothCore currently supports module-owned databases through `ModuleDatabasePool` and `DatabaseScript` lifecycle hooks.
- AzerothCore currently exposes PlayerScript hooks needed for the planned event collector, including quest completion, creature kills, level/zone changes and item creation/store callbacks.

These observations must be rechecked against the exact commits pinned by the real FURY repository during T00.2.

## Next action

When the FURY Azeroth repository is available to the session, perform T00.1 immediately:

1. inspect git state without modifying it;
2. record exact core/module revisions;
3. verify build and DB entrypoints;
4. verify APIs in the pinned source;
5. only then begin `mod-fury` T01.
