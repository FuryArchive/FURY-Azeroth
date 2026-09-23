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

mkdir -p "${WORKDIR}/lw"
git -C "${WORKDIR}/lw" apply --include=src/invasions/RuntimeCompletionObserver.h "${ROOT}/vendor/patches/mod-living-world/0001-fury-bridge.patch"
CXX="${CXX:-g++}"

"${CXX}" \
  -std=c++17 \
  -Wall -Wextra -Werror \
  -I"${WORKDIR}" \
  -I"${WORKDIR}/lw/src/invasions" \
  -I"${ROOT}/modules/mod-fury/src" \
  "${ROOT}/tests/golden/living_world_completion_golden.cpp" \
  -o "${WORKDIR}/living_world_completion_golden"

"${WORKDIR}/living_world_completion_golden"
