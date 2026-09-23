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

# Only external formatting is stubbed; event payload/identity formatting is
# covered by integration tests, not this persistence-failure regression.
cat >"${WORKDIR}/StringFormat.h" <<'STUB'
#pragma once
#include <string>
namespace Acore { template<class... T> std::string StringFormat(char const* format, T const&...) { return format; } }
STUB
CXX="${CXX:-g++}"

"${CXX}" \
  -std=c++17 \
  -Wall -Wextra -Werror \
  -I"${WORKDIR}" \
  -I"${ROOT}/modules/mod-fury/src" \
  "${ROOT}/modules/mod-fury/src/director/DirectorService.cpp" \
  "${ROOT}/tests/golden/director_replay_golden.cpp" \
  -o "${WORKDIR}/director_replay_golden"

"${WORKDIR}/director_replay_golden"
