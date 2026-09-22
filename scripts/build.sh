#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="${ROOT}/upstream/azerothcore-wotlk"
BUILD_DIR="${ROOT}/build/azerothcore"
INSTALL_DIR="${ROOT}/build/dist"

if [[ ! -f "${CORE}/CMakeLists.txt" ]]; then
  echo "[FURY] AzerothCore workspace missing. Run: bash scripts/sync-upstreams.sh" >&2
  exit 1
fi

mkdir -p "${BUILD_DIR}" "${INSTALL_DIR}"

JOBS="${FURY_BUILD_JOBS:-2}"
C_COMPILER="${CC:-clang}"
CXX_COMPILER="${CXX:-clang++}"

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
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
  -DCMAKE_C_COMPILER="${C_COMPILER}" \
  -DCMAKE_CXX_COMPILER="${CXX_COMPILER}" \
  -DWITH_WARNINGS=1 \
  -DTOOLS_BUILD=none \
  -DSCRIPTS=static \
  -DMODULES=static \
  -DAPPS_BUILD=all \
  "${CMAKE_LAUNCHER_ARGS[@]}"

echo "[FURY] compile module target first"
cmake --build "${BUILD_DIR}" --target modules --parallel "${JOBS}"

echo "[FURY] build full server with ${JOBS} job(s)"
cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

if command -v ccache >/dev/null 2>&1; then
  ccache --show-stats || true
fi

echo "[FURY] build complete"
