#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "${WORKDIR}"' EXIT

cat >"${WORKDIR}/Define.h" <<'EOF'
#pragma once
#include <cstdint>
using uint8 = std::uint8_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
EOF

cat >"${WORKDIR}/ObjectGuid.h" <<'EOF'
#pragma once
class ObjectGuid
{
public:
    static ObjectGuid const Empty;
};

inline ObjectGuid const ObjectGuid::Empty{};
EOF

CXX="${CXX:-g++}"

"${CXX}" \
  -std=c++17 \
  -Wall -Wextra -Werror \
  -I"${WORKDIR}" \
  -I"${ROOT}/modules/mod-fury/src/actors" \
  "${ROOT}/tests/golden/actor_policy_golden.cpp" \
  -o "${WORKDIR}/actor_policy_golden"

"${WORKDIR}/actor_policy_golden"
