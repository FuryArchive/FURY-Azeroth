#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="${ROOT}/upstream/azerothcore-wotlk"
LW="${CORE}/modules/mod-living-world"
PATCH="${ROOT}/vendor/patches/mod-living-world/0001-fury-bridge.patch"
EXPECTED_PIN="116926ef9ce42ee0bba223c1e406cf62cedd9904"

bash "${ROOT}/scripts/sync-upstreams.sh"

actual_pin="$(git -C "${LW}" rev-parse HEAD)"
if [[ "${actual_pin}" != "${EXPECTED_PIN}" ]]; then
  echo "[FURY][FAIL] Living World pin mismatch: expected ${EXPECTED_PIN}, got ${actual_pin}" >&2
  exit 1
fi

echo "[FURY][PASS] Living World exact pin resolved"

changed="$(git -C "${LW}" diff --name-only | sort)"
expected=
if [[ "${changed}" != "${expected}" ]]; then
  echo "[FURY][FAIL] unexpected Living World patch surface:" >&2
  printf '%s\n' "${changed}" >&2
  exit 1
fi

echo "[FURY][PASS] Living World changes are limited to the bridge plus pinned Defias SQL compatibility"

grep -Fq "struct RuntimeEntityMetadata" "${LW}/src/core/RuntimeEntityGroup.h"
grep -Fq "FindEntityMetadata(ObjectGuid guid, RuntimeEntityMetadata& metadata) const" "${LW}/src/core/RuntimeEntityGroup.h"
grep -Fq "RuntimeEntityGroupManager::FindEntityMetadata" "${LW}/src/core/RuntimeEntityGroup.cpp"

echo "[FURY][PASS] reverse runtime-entity lookup surface exists"
test -f "${LW}/src/invasions/RuntimeCompletionObserver.h"
grep -Fq "SetCompletionObserver" "${LW}/src/invasions/InvasionRuntimeManager.h"
grep -Fq "if (!NotifyCompletion(iterator->second, true))" "${LW}/src/invasions/InvasionRuntimeManager.cpp"
grep -Fq "if (!NotifyCompletion(iterator->second, false))" "${LW}/src/invasions/InvasionRuntimeManager.cpp"
bash "${ROOT}/scripts/test-t25-lw-completion.sh"

if grep -Eiq 'Household|Contract|Chronicle|Reward|Campaign|Director' "${PATCH}"; then
  echo "[FURY][FAIL] Living World bridge leaked FURY domain concepts" >&2
  exit 1
fi

echo "[FURY][PASS] bridge remains domain-neutral"

# Prove the patched upstream compiles without mod-fury being present.
rm -f "${CORE}/modules/mod-fury"
FURY_BUILD_TARGET=living-world-only FURY_BUILD_JOBS="${FURY_BUILD_JOBS:-4}" bash "${ROOT}/scripts/build.sh"

echo "[FURY][PASS] T22 Living World bridge compatibility/compile gate passed"
data/sql/db-world/prebuilt/900_defias_westfall_invasion.sql\nsrc/core/RuntimeEntityGroup.cpp\nsrc/core/RuntimeEntityGroup.h\nsrc/invasions/InvasionRuntimeManager.cpp\nsrc/invasions/InvasionRuntimeManager.h'
if [[ "${changed}" != "${expected}" ]]; then
  echo "[FURY][FAIL] unexpected Living World patch surface:" >&2
  printf '%s\n' "${changed}" >&2
  exit 1
fi

echo "[FURY][PASS] bridge patch touches only runtime entity metadata and terminal observer APIs"

grep -Fq "struct RuntimeEntityMetadata" "${LW}/src/core/RuntimeEntityGroup.h"
grep -Fq "FindEntityMetadata(ObjectGuid guid, RuntimeEntityMetadata& metadata) const" "${LW}/src/core/RuntimeEntityGroup.h"
grep -Fq "RuntimeEntityGroupManager::FindEntityMetadata" "${LW}/src/core/RuntimeEntityGroup.cpp"

echo "[FURY][PASS] reverse runtime-entity lookup surface exists"
test -f "${LW}/src/invasions/RuntimeCompletionObserver.h"
grep -Fq "SetCompletionObserver" "${LW}/src/invasions/InvasionRuntimeManager.h"
grep -Fq "if (!NotifyCompletion(iterator->second, true))" "${LW}/src/invasions/InvasionRuntimeManager.cpp"
grep -Fq "if (!NotifyCompletion(iterator->second, false))" "${LW}/src/invasions/InvasionRuntimeManager.cpp"
bash "${ROOT}/scripts/test-t25-lw-completion.sh"

if grep -Eiq 'Household|Contract|Chronicle|Reward|Campaign|Director' "${PATCH}"; then
  echo "[FURY][FAIL] Living World bridge leaked FURY domain concepts" >&2
  exit 1
fi

echo "[FURY][PASS] bridge remains domain-neutral"

# Prove the patched upstream compiles without mod-fury being present.
rm -f "${CORE}/modules/mod-fury"
FURY_BUILD_TARGET=living-world-only FURY_BUILD_JOBS="${FURY_BUILD_JOBS:-4}" bash "${ROOT}/scripts/build.sh"

echo "[FURY][PASS] T22 Living World bridge compatibility/compile gate passed"
