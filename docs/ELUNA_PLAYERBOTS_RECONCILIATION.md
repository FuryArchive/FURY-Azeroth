# Standard Eluna + Playerbots Reconciliation

The selected Delves and MythicPlus Extended integrations require standard Eluna semantics. FURY's canonical core is the pinned Playerbots AzerothCore fork, so replacing the core with ElunaAzerothcore is not acceptable.

Both forks share AzerothCore history. FURY therefore treats standard Eluna as a **core merge source**, not as a parallel runtime.

`scripts/audit-eluna-playerbots-merge.sh`:

1. resolves the pinned Playerbots core;
2. fetches the pinned `ElunaLuaEngine/ElunaAzerothcore` commit;
3. computes the real git merge-base;
4. performs a no-commit three-way merge;
5. records the exact conflict set;
6. aborts the temporary merge and leaves the generated workspace clean.

This converts the problem from "port Eluna manually" into a finite conflict-resolution queue.

The audit is intentionally red while unresolved conflicts exist. Once those conflicts have explicit FURY resolutions, the same gate becomes the regression check for future Playerbots/Eluna pin updates.

This reconciliation unlocks two selected content tracks at once:

- Delves: boss/teleporter Lua scripts;
- MythicPlus Extended: standard Eluna + AIO execution.

Stock AzerothCore `mod-ale` remains excluded from these integrations because it is not script-compatible with the standard Eluna APIs they target.
