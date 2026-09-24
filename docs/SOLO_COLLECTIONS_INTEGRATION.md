# FURY Azeroth — SoloCollections Integration

Date: 2026-09-24

SoloCollectionsPlatform is the canonical collection/transmog stack for FURY Azeroth. The entire integration is pinned to one platform revision so server catalog, protocol, client metadata and UI stay matched.

## Ownership

FURY uses the platform's C++/SC2 production path:

- backend: `mod-solo-collections`;
- protocol: SC2;
- client: `SoloCollections`;
- backend mode: `SoloCollections.Backend = Cpp`;
- preview: `SoloCollections.Preview.Enabled = 1`.

The ALE/SC1 backend is migration-only and is not a FURY production writer.

Legacy `mod-transmog` and separate account-mount collection modules are not stacked. The matched backend already retains compatible transmogrification functionality and owns account collection state.

## Server wiring

With:

```bash
FURY_STACK_PROFILE=all bash scripts/sync-upstreams.sh
```

FURY clones the pinned SoloCollectionsPlatform workspace and links:

```text
upstream/integrations/SoloCollectionsPlatform/mod-solo-collections
  -> upstream/azerothcore-wotlk/modules/mod-solo-collections
```

This keeps the backend source physically inside the matched platform checkout while exposing the standard AzerothCore module path.

A fast backend compile is available with:

```bash
FURY_BUILD_TARGET=module-only \
FURY_SOURCE_MODULE=mod-solo-collections \
bash scripts/build.sh
```

## Client dependency chain

The matched SoloCollections AddOn declares `DragonUI_NewEra` as a hard dependency. The matched New Era base declares both `DragonUI` and `!!!ClassicAPI`.

Therefore the minimum FURY collection client stack is:

```text
!!!ClassicAPI
DragonUI
DragonUI_NewEra
SoloCollections
```

`DragonUI_Options` is staged when present as an optional matched configuration UI.

All of these paths come from the same pinned SoloCollectionsPlatform revision. Do not replace one component with an unrelated release.

## Client staging

Run:

```bash
bash scripts/stage-client-integrations.sh
```

Output:

```text
build/client-integrations/
  FURY-SOLO-COLLECTIONS-MANIFEST.txt
  Interface/
    AddOns/
      !!!ClassicAPI/
      DragonUI/
      DragonUI_NewEra/
      DragonUI_Options/   # optional
      SoloCollections/
```

The staging script writes only to the build workspace. It does not modify a real WoW client.

This directory is the input for the later unified FURY/Reforged client-pack assembler.

## Acceptance

`scripts/test-solo-collections-integration.sh` verifies:

- the matched backend is linked into AzerothCore;
- C++/SC2 is the production backend default;
- server preview is enabled;
- the hard AddOn dependency chain has not changed;
- all required matched client AddOns exist;
- a clean staging tree and manifest can be produced.

`.github/workflows/solo-collections-integration.yml` then fast-compiles only `mod-solo-collections`.

Runtime/database acceptance remains a later packaging step. The server must eventually report the SoloCollections startup/schema/provider ready diagnostics and a real client must complete the SC2 handshake before this integration becomes runtime-GREEN.
