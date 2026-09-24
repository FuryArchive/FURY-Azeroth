# FURY Azeroth — Playable Profile

Date: 2026-09-24

The playable profile is the one supported path for sitting down and playing FURY Azeroth end to end.

## Included

Server:

- pinned Playerbots AzerothCore;
- standard Eluna reconciled into the Playerbots core;
- FURY first-party module;
- all selected server modules from the lock;
- SoloCollections C++ backend;
- playable Goblin/Worgen core + module support;
- Delves server maps, vmaps, mmaps, SQL and Lua;
- MythicPlus Extended SQL/Lua;
- AIO server runtime;
- authserver + worldserver;
- FURY authority guards for overlapping reward/progression systems.

Client overlay:

- Worgen/Goblin HD-compatible content baseline;
- merged DBCs for Worgen/Goblin + Delves + Mythic+;
- Delves maps/models/textures/sounds;
- Mythic+ client assets;
- SoloCollections + matched DragonUI dependency stack;
- AIO client;
- DelvesTeleporter AddOn;
- one generated `Data/patch-Z.MPQ`.

Upstream-declared WIP Delves are staged for future development but hidden from the production teleporter.

## CI acceptance

`.github/workflows/playable-profile.yml` performs one full acceptance pass:

1. resolve selected modules and the five playable integrations;
2. reconcile standard Eluna into the patched Playerbots core;
3. build authserver + worldserver once;
4. install pinned AzerothCore runtime data;
5. merge Worgen HD DBCs with Delves/Mythic CSV deltas;
6. stage and validate server/client payloads;
7. build and inspect the final WotLK MPQ;
8. start worldserver once to initialize core/module databases;
9. import Delves + Mythic+ SQL into real MySQL;
10. start authserver and verify port 3724;
11. start worldserver again with AIO/Mythic+/Delves Lua enabled;
12. assemble standalone server and client artifacts;
13. bundle non-glibc runtime libraries and the offline LLM Chatter bridge;
14. build the packaged LLM bridge container image;
15. extract and bootstrap the actual server tarball against isolated MySQL;
16. install the actual client ZIP onto a clean 3.3.5a-shaped tree;
17. upload the accepted server and client artifacts.

A green playable-profile run is the release gate for a playable build.

## Local installation from CI artifacts

Download and extract `FURY-Azeroth-Server`.

The server bundle includes Docker Compose for an isolated MySQL 8 database, persistent progress storage, backup/restore tooling, sequential Classic-to-WotLK progression, and the optional packaged LLM Chatter bridge. From inside the extracted `FURY-Azeroth` directory:

```bash
bash runtime/bootstrap-playable.sh fury fury
bash runtime/run-playable.sh
```

The bootstrap initializes a fresh realm at the first FURY progression bracket, creates the requested account, and configures LLM Chatter for a detected local Ollama model when available. LLM absence is non-fatal.

The bootstrap chooses the first non-loopback host address as the realm address when available. Override it for LAN/VPN play with:

```bash
FURY_REALM_ADDRESS=192.168.1.50 bash runtime/bootstrap-playable.sh fury your-password
```

Download and extract `FURY-Azeroth-Client`. Install it into an existing 3.3.5a Project Reforged client:

```bash
bash runtime/install-client.sh /path/to/WoW-3.3.5a 192.168.1.50
```

The installer:

- backs up an existing `Data/patch-Z.MPQ` instead of silently overwriting it;
- installs the FURY MPQ;
- installs the matched AddOns;
- updates existing `realmlist.wtf` files, or creates an enUS one when none exists.

Then launch the WoW client normally and log in using the account passed to `bootstrap-playable.sh`.

For normal play/update operations, use the commands documented in the packaged `PLAYABLE.md`: `backup-playable.sh`, `restore-playable.sh`, `advance-progression.sh`, and `configure-llm-chatter.sh`.

## Source build

For engineering/debugging:

```bash
FURY_STACK_PROFILE=all \
FURY_INTEGRATION_FILTER=solo_collections_platform,worgoblin,delves,mythic_plus_extended,aio \
bash scripts/sync-upstreams.sh

bash scripts/resolve-eluna-playerbots.sh

FURY_APPS_BUILD=all \
FURY_BUILD_JOBS=4 \
bash scripts/build.sh

bash scripts/fetch-ac-data.sh
bash scripts/stage-playable-profile.sh
bash scripts/test-playable-profile.sh
bash scripts/build-playable-client-mpq.sh
```

Do not install the raw upstream Worgen/Delves/Mythic DBCs independently. The FURY-generated DBC overlay is the authority because it merges all three selected client-content systems.
