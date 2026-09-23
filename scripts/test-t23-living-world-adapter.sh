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
  "${ROOT}/tests/golden/living_world_adapter_policy_golden.cpp" \
  -o "${WORKDIR}/living_world_adapter_policy_golden"

"${WORKDIR}/living_world_adapter_policy_golden"

ADAPTER="${ROOT}/modules/mod-fury/src/integrations/LivingWorldAdapter.cpp"

grep -Fq 'sInvasionScheduler.TriggerInvasion' "${ADAPTER}"
grep -Fq 'sInvasionRuntimeMgr.GetRuntimeForInvasion' "${ADAPTER}"
grep -Fq 'sRuntimeSignalMgr.EmitSignal' "${ADAPTER}"
grep -Fq 'sRuntimeEntityGroupMgr.FindEntityMetadata' "${ADAPTER}"

if grep -Fq 'sInvasionRuntimeMgr.StartInvasion' "${ADAPTER}"; then
  echo "[FURY][FAIL] adapter bypasses scheduler-owned controlled start" >&2
  exit 1
fi

violations="$(
  grep -R -n -E     'sInvasionScheduler|sInvasionRuntimeMgr|sRuntimeSignalMgr|sRuntimeEntityGroupMgr|sLivingWorldDataMgr|#include "(InvasionScheduler|InvasionRuntime|InvasionRuntimeManager|LivingWorld|RuntimeEntityGroup|RuntimeSignalManager)\.h"'     "${ROOT}/modules/mod-fury/src"     --include='*.h' --include='*.cpp' \
    | grep -v '/integrations/LivingWorldAdapter.cpp:' || true
)"

if [[ -n "${violations}" ]]; then
  echo "[FURY][FAIL] Living World API escaped the adapter boundary:" >&2
  printf '%s\n' "${violations}" >&2
  exit 1
fi

grep -Fq '_defiasRecovery.Reconcile()'   "${ROOT}/modules/mod-fury/src/core/FuryApp.cpp"
grep -Fq '_livingWorld.Inspect(run)'   "${ROOT}/modules/mod-fury/src/content/defias/DefiasRecoveryService.cpp"
grep -Fq '_livingWorld.AbortRuntime'   "${ROOT}/modules/mod-fury/src/content/defias/DefiasRecoveryService.cpp"

echo "[FURY][PASS] Living World calls remain isolated behind the adapter"
echo "[FURY][PASS] Director reconciliation executes through the Living World adapter"
echo "[FURY][PASS] T23 Living World adapter boundary gate passed"
