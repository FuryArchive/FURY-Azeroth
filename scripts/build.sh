#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="${ROOT}/upstream/azerothcore-wotlk"
BUILD_DIR="${ROOT}/build/azerothcore"
INSTALL_DIR="${ROOT}/build/dist"
LOCK="${ROOT}/vendor/lock/fury.lock.yaml"

if [[ ! -f "${CORE}/CMakeLists.txt" ]]; then
  echo "[FURY] AzerothCore workspace missing. Run: bash scripts/sync-upstreams.sh" >&2
  exit 1
fi

mkdir -p "${BUILD_DIR}" "${INSTALL_DIR}"

JOBS="${FURY_BUILD_JOBS:-2}"
C_COMPILER="${CC:-clang}"
CXX_COMPILER="${CXX:-clang++}"

TARGET="${FURY_BUILD_TARGET:-}"
BUILD_TYPE="${FURY_BUILD_TYPE:-RelWithDebInfo}"
APPS_BUILD="${FURY_APPS_BUILD:-world-only}"

CMAKE_GENERATOR_ARGS=()
if command -v ninja >/dev/null 2>&1; then
  CMAKE_GENERATOR_ARGS+=("-G" "Ninja")
fi

CMAKE_FAST_ARGS=()
if [[ "${TARGET}" == "fury-only" || "${TARGET}" == "living-world-only" || "${TARGET}" == "selected-modules-only" || "${TARGET}" == "integration-module-only" ]]; then
  CMAKE_FAST_ARGS+=(
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    "-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON"
  )
fi

CMAKE_LAUNCHER_ARGS=()
if command -v ccache >/dev/null 2>&1; then
  echo "[FURY] ccache enabled"
  CMAKE_LAUNCHER_ARGS+=(
    "-DCMAKE_C_COMPILER_LAUNCHER=ccache"
    "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
  )
fi

echo "[FURY] configure"
cmake -S "${CORE}" -B "${BUILD_DIR}" \
  "${CMAKE_GENERATOR_ARGS[@]}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
  -DCMAKE_C_COMPILER="${C_COMPILER}" \
  -DCMAKE_CXX_COMPILER="${CXX_COMPILER}" \
  -DWITH_WARNINGS=1 \
  -DTOOLS_BUILD=none \
  -DSCRIPTS=static \
  -DMODULES=static \
  -DAPPS_BUILD="${APPS_BUILD}" \
  "${CMAKE_LAUNCHER_ARGS[@]}" \
  "${CMAKE_FAST_ARGS[@]}"

compile_module_tus() {
  local module="$1"
  echo "[FURY] compile ${module} translation units only"
  FURY_BUILD_JOBS="${JOBS}" FURY_SOURCE_MODULE="${module}" python3 "${ROOT}/scripts/compile-fury-only.py" \
    "${BUILD_DIR}/compile_commands.json"
}

if [[ "${TARGET}" == "fury-only" ]]; then
  compile_module_tus "mod-fury"
elif [[ "${TARGET}" == "living-world-only" ]]; then
  compile_module_tus "mod-living-world"
elif [[ "${TARGET}" == "selected-modules-only" ]]; then
  if [[ ! -f "${LOCK}" ]]; then
    echo "[FURY] missing selected-stack lock: ${LOCK}" >&2
    exit 1
  fi

  mapfile -t selected_modules < <(python3 - "${LOCK}" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as fh:
    lock = json.load(fh)

for module in lock["modules"].values():
    if module.get("tier") == "selected":
        print(module["directory"])
PY
)

  if [[ "${#selected_modules[@]}" -eq 0 ]]; then
    echo "[FURY] selected module list is empty" >&2
    exit 1
  fi

  echo "[FURY] fast selected-stack compile: ${#selected_modules[@]} module(s)"
  failed_modules=()
  for module in "${selected_modules[@]}"; do
    if compile_module_tus "${module}"; then
      echo "[FURY][PASS] selected-stack module ${module}"
    else
      echo "[FURY][FAIL] selected-stack module ${module}" >&2
      failed_modules+=("${module}")
    fi
  done

  if [[ "${#failed_modules[@]}" -ne 0 ]]; then
    echo "[FURY][FAIL] selected-stack compile failures: ${#failed_modules[@]}" >&2
    printf '  - %s\n' "${failed_modules[@]}" >&2
    exit 1
  fi

  echo "[FURY][PASS] all selected server modules compile"
elif [[ "${TARGET}" == "integration-module-only" ]]; then
  module="${FURY_INTEGRATION_MODULE:-}"
  if [[ -z "${module}" ]]; then
    echo "[FURY] FURY_INTEGRATION_MODULE is required for integration-module-only" >&2
    exit 1
  fi
  compile_module_tus "${module}"

  if [[ -n "${FURY_INTEGRATION_CORE_FILES:-}" ]]; then
    echo "[FURY] compile integration-touched core translation units"
    FURY_BUILD_JOBS="${JOBS}" \
      FURY_SOURCE_MODULE="core-integration" \
      FURY_SOURCE_FILES="${FURY_INTEGRATION_CORE_FILES}" \
      python3 "${ROOT}/scripts/compile-fury-only.py" "${BUILD_DIR}/compile_commands.json"
  fi
elif [[ -n "${TARGET}" ]]; then
  echo "[FURY] build target '${TARGET}' with ${JOBS} job(s)"
  cmake --build "${BUILD_DIR}" --target "${TARGET}" --parallel "${JOBS}"
else
  echo "[FURY] build worldserver dependency graph with ${JOBS} job(s)"
  cmake --build "${BUILD_DIR}" --target worldserver --parallel "${JOBS}"

  echo "[FURY] install worldserver runtime artifacts"
  cmake --install "${BUILD_DIR}"

  echo "[FURY] worldserver binary smoke"
  "${INSTALL_DIR}/bin/worldserver" --version
fi

if command -v ccache >/dev/null 2>&1; then
  ccache --show-stats || true
fi

echo "[FURY] build complete"
