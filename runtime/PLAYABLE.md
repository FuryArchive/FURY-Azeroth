# FURY Azeroth - playable package

This package is intended to run without a local AzerothCore build tree.

## Server

Requirements:
- modern x86_64 Linux
- Docker with the Compose plugin
- Python 3

The archive bundles the non-glibc shared libraries used by the CI-built server, so matching host versions of Boost, MySQL client libraries, OpenSSL, and similar packages are not required.

From the extracted `FURY-Azeroth` directory:

```bash
./runtime/bootstrap-playable.sh fury fury
./runtime/run-playable.sh
```

`bootstrap-playable.sh` starts the bundled MySQL service, initializes the AzerothCore/module databases, imports the staged Delves and Mythic+ data, creates the requested game account, and writes runtime configuration files.

The first two positional arguments are the game account name and password. If omitted, both default to `fury`.

The bootstrap is repeat-safe for the packaged one-time Delves/Mythic+ content. Run it again with another account name/password to create the second co-op account without re-importing that content:

```bash
./runtime/bootstrap-playable.sh fury2 choose-a-password
```

### Staged Classic -> TBC -> WotLK progression

A fresh FURY realm starts in Progression System `Bracket_0`. Do not edit the progression module config by hand.

When the current progression milestone is complete and the realm is stopped, advance exactly one bracket with:

```bash
./runtime/advance-progression.sh
```

The command creates a database backup first, enables only the next upstream bracket, starts worldserver long enough to apply its database updates, records the new FURY progression state, and shuts worldserver down again. It will not skip brackets or move backwards.

Useful environment variables:

```bash
export FURY_MYSQL_PASSWORD='change-me'
export FURY_MYSQL_PORT=3306
export FURY_REALM_ADDRESS='192.168.1.10'
```

`run-playable.sh` refuses to start until bootstrap has completed.

The MySQL data lives in the stable Docker volume `fury-azeroth-mysql`, so extracting a newer FURY server package does not by itself discard character/server progress.

Before an update, create a portable database backup:

```bash
./runtime/backup-playable.sh
```

Backups are written to `./backups/` by default. To restore one while the realm is stopped:

```bash
./runtime/restore-playable.sh ./backups/FURY-Azeroth-backup-YYYYMMDD-HHMMSS.tar.gz
```

### LLM Chatter

The server archive includes the pinned Python LLM Chatter bridge and all of its Python dependencies as an offline wheelhouse. No separate bridge checkout or `pip install` is required.

During bootstrap, FURY checks the local Ollama service. If an installed non-embedding model is found, chatter is configured for Ollama automatically, preferring a roughly 4-8B model when available. If Ollama is absent or has no model installed, LLM Chatter is disabled and the realm starts normally.

To enable it later, install/pull the Ollama model you want, stop the realm, then run:

```bash
FURY_LLM_MODEL='<ollama-model>' ./runtime/configure-llm-chatter.sh
./runtime/run-playable.sh
```

To force chatter off:

```bash
FURY_LLM_ENABLE=0 ./runtime/configure-llm-chatter.sh
```

`run-playable.sh` starts the packaged bridge automatically only when chatter is enabled. A bridge startup failure is non-fatal to the WoW realm.

## Client

Extract `FURY-Azeroth-Client.zip`, then run:

```bash
./FURY-Azeroth-Client/runtime/install-client.sh "/path/to/WoW-3.3.5a" "127.0.0.1"
```

The installer:
- installs the FURY MPQ overlay without modifying the original Project Reforged MPQs;
- installs the matched client AddOns;
- updates the client's `realmlist.wtf`;
- backs up an existing `Data/patch-Z.MPQ` before replacing it.

Use the server machine's LAN address instead of `127.0.0.1` when the client runs on another computer.

## Included playable integrations

The unified profile contains the selected Playerbots server stack plus FURY campaign systems, SoloCollections, Goblin/Worgen, Delves, Mythic+ Extended/AIO, and the matched client payload.

Project Reforged remains the graphics foundation; FURY installs as an overlay on top of it.
