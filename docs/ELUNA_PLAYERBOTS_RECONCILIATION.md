# Standard Eluna + Playerbots Reconciliation

The selected Delves and MythicPlus Extended integrations require standard Eluna semantics. FURY's canonical core remains the pinned Playerbots AzerothCore fork.

A real three-way merge audit found twelve conflicts at the current pins:

- nine conflicts are upstream GitHub workflow files;
- three are runtime files: `worldserver.conf.dist`, `Object.cpp`, and `Object.h`.

The FURY resolution is deterministic:

- all upstream workflow conflicts keep the Playerbots side;
- `worldserver.conf.dist` keeps Playerbots logging/mail settings and adds Eluna logging/runtime settings;
- `Object.cpp` keeps Playerbots WorldObject spell/faction extensions and adds Eluna engine access/event processors;
- `Object.h` keeps Playerbots WorldObject APIs and adds the matching Eluna state/API.

All other standard-Eluna changes merge automatically from the shared AzerothCore history.

`scripts/resolve-eluna-playerbots.sh` performs the merge in the generated upstream workspace, commits the synthetic result locally, initializes the pinned Eluna submodule, and leaves FURY's own repository untouched.

The reconciliation CI compiles the merged `game` target. Once green, this synthetic core becomes the execution base for both Delves Lua scripts and MythicPlus Extended/AIO testing.

Stock `mod-ale` remains excluded from those integrations because their scripts target standard Eluna APIs.
