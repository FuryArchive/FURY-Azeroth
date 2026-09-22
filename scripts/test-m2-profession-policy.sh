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
  "${ROOT}/tests/golden/profession_order_policy_golden.cpp" \
  -o "${WORKDIR}/profession_order_policy_golden"

"${WORKDIR}/profession_order_policy_golden"

grep -Fq 'ProfessionCrafted(' "${ROOT}/modules/mod-fury/src/scripts/FuryPlayerScript.cpp"
grep -Fq 'ResolveProfessionSkill' "${ROOT}/modules/mod-fury/src/scripts/FuryPlayerScript.cpp"
grep -Fq 'FORCE INDEX (ix_fury_profession_order_option_target)' \
  "${ROOT}/modules/mod-fury/src/database/FuryDatabase.cpp"

echo "[FURY][PASS] craft hook and indexed target lookup are wired"
