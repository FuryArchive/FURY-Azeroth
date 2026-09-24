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
