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

TARGET="${FURY_BUILD_TARGET:-}"

CMAKE_FAST_ARGS=()
if [[ "${TARGET}" == "fury-only" ]]; then
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
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
  -DCMAKE_C_COMPILER="${C_COMPILER}" \
  -DCMAKE_CXX_COMPILER="${CXX_COMPILER}" \
  -DWITH_WARNINGS=1 \
  -DTOOLS_BUILD=none \
  -DSCRIPTS=static \
  -DMODULES=static \
  -DAPPS_BUILD=all \
  "${CMAKE_LAUNCHER_ARGS[@]}" \
  "${CMAKE_FAST_ARGS[@]}"

if [[ "${TARGET}" == "fury-only" ]]; then
  echo "[FURY] compile mod-fury translation units only"
  FURY_BUILD_JOBS="${JOBS}" python3 "${ROOT}/scripts/compile-fury-only.py" \
    "${BUILD_DIR}/compile_commands.json"
elif [[ -n "${TARGET}" ]]; then
  echo "[FURY] build target '${TARGET}' with ${JOBS} job(s)"
  cmake --build "${BUILD_DIR}" --target "${TARGET}" --parallel "${JOBS}"
else
  echo "[FURY] compile module target first"
  cmake --build "${BUILD_DIR}" --target modules --parallel "${JOBS}"

  echo "[FURY] build full server with ${JOBS} job(s)"
  cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

  echo "[FURY] install build artifacts"
  cmake --install "${BUILD_DIR}"
fi

if command -v ccache >/dev/null 2>&1; then
  ccache --show-stats || true
fi

echo "[FURY] build complete"
