#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "${WORKDIR}"' EXIT

cat >"${WORKDIR}/Define.h" <<'EOF'
#pragma once
#include <cstdint>
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
EOF

cat >"${WORKDIR}/ObjectGuid.h" <<'EOF'
#pragma once
class ObjectGuid
{
public:
    static ObjectGuid const Empty;
    bool IsEmpty() const { return true; }
    unsigned long long GetRawValue() const { return 0; }
};
inline ObjectGuid const ObjectGuid::Empty{};
EOF

cat >"${WORKDIR}/Player.h" <<'EOF'
#pragma once
class Player
{
public:
    bool IsInWorld() const { return true; }
};
EOF

cat >"${WORKDIR}/World.h" <<'EOF'
#pragma once
EOF

CXX="${CXX:-g++}"

"${CXX}" \
  -std=c++17 \
  -Wall -Wextra -Werror \
  -I"${WORKDIR}" \
  -I"${ROOT}/modules/mod-fury/src" \
  "${ROOT}/tests/golden/ip_access_policy_golden.cpp" \
  -o "${WORKDIR}/ip_access_policy_golden"

"${WORKDIR}/ip_access_policy_golden"

# Compile the adapter with no IndividualProgression.h on the include path.
# This proves the optional-module fallback is a real compile path rather than
# an untested preprocessor branch.
"${CXX}" \
  -std=c++17 \
  -Wall -Wextra -Werror \
  -I"${WORKDIR}" \
  -I"${ROOT}/modules/mod-fury/src" \
  -c "${ROOT}/modules/mod-fury/src/integrations/IndividualProgressionAdapter.cpp" \
  -o "${WORKDIR}/IndividualProgressionAdapter.no-ip.o"

ADAPTER="${ROOT}/modules/mod-fury/src/integrations/IndividualProgressionAdapter.cpp"

for forbidden in   "UpdateProgressionState"   "ForceUpdateProgressionState"   "UpdatePlayerSetting"   "CharacterDatabase"   "WorldDatabase"   "LoginDatabase"   "GetPreparedStatement"; do
  if grep -Fq "${forbidden}" "${ADAPTER}"; then
    echo "[FURY][FAIL] IP adapter contains forbidden mutation/database API: ${forbidden}" >&2
    exit 1
  fi
done

grep -Fq '__has_include("IndividualProgression.h")' "${ADAPTER}"
grep -Fq "GetPlayerProgressionFromQuests" "${ADAPTER}"
grep -Fq "ModuleIncompatible" "${ADAPTER}"

echo "[FURY][PASS] IP adapter is optional, read-only, and fail-closed"
