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

CXX="${CXX:-g++}"

"${CXX}" \
  -std=c++17 \
  -Wall -Wextra -Werror \
  -I"${WORKDIR}" \
  -I"${ROOT}/modules/mod-fury/src" \
  "${ROOT}/tests/golden/defias_participation_policy_golden.cpp" \
  -o "${WORKDIR}/defias_participation_policy_golden"

"${WORKDIR}/defias_participation_policy_golden"

SERVICE="${ROOT}/modules/mod-fury/src/content/defias/DefiasParticipation.cpp"
SCRIPT="${ROOT}/modules/mod-fury/src/scripts/FuryPlayerScript.cpp"
FACTORY="${ROOT}/modules/mod-fury/src/events/FuryEventFactory.cpp"

grep -Fq 'Fury.Defias.ParticipationWindowSeconds' "${SERVICE}"
grep -Fq 'Fury.Defias.ParticipationRadiusYards' "${SERVICE}"
grep -Fq 'Fury.Defias.ParticipationGroupShare' "${SERVICE}"
grep -Fq 'GetCharmerOrOwnerPlayerOrPlayerItself' "${SERVICE}"
grep -Fq 'UNITHOOK_ON_DAMAGE' "${SCRIPT}"
grep -Fq 'UNITHOOK_ON_UNIT_DEATH' "${SCRIPT}"

if grep -Fq 'PublishLivingWorldKill' "${SCRIPT}"; then
  echo "[FURY][FAIL] legacy last-hit runtime publication still exists" >&2
  exit 1
fi

if grep -Fq 'LivingWorldEntityKilled(' "${SCRIPT}"; then
  echo "[FURY][FAIL] PlayerScript still authors runtime kill progress directly" >&2
  exit 1
fi

grep -Fq 'living-world:entity-killed:v2:' "${FACTORY}"
grep -Fq 'credit_kind' "${FACTORY}"
grep -Fq 'direct_participant_count' "${FACTORY}"

echo "[FURY][PASS] T28 human participation resolver gate passed"
