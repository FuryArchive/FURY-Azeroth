#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${FURY_AC_DATA_VERSION:-v20.0}"
EXPECTED_VERSION="v20.0"
EXPECTED_SHA256="a3d4df635ae6c2c8f08052c32a79e0f806955150ad36b014a823dd08a32a4610"
URL="https://github.com/wowgaming/client-data/releases/download/v20.0/Data.zip"

CACHE_DIR="${FURY_AC_DATA_CACHE_DIR:-${ROOT}/.cache/ac-data-v20.0}"
ZIP="${CACHE_DIR}/Data.zip"
DATA_DIR="${FURY_AC_DATA_DIR:-${ROOT}/build/dist/data}"
STAGE="$(mktemp -d)"
trap 'rm -rf "${STAGE}"' EXIT

if [[ "${VERSION}" != "${EXPECTED_VERSION}" ]]; then
  echo "[FURY][FAIL] unsupported AC data version: ${VERSION}; expected ${EXPECTED_VERSION}" >&2
  exit 2
fi

mkdir -p "${CACHE_DIR}"

verify_zip() {
  [[ -f "${ZIP}" ]] || return 1
  local actual
  actual="$(sha256sum "${ZIP}" | awk '{print $1}')"
  [[ "${actual}" == "${EXPECTED_SHA256}" ]]
}

if ! verify_zip; then
  rm -f "${ZIP}"
  echo "[FURY] downloading AzerothCore client data ${VERSION}"
  curl --fail --location --retry 5 --retry-delay 3 \
    --output "${ZIP}.tmp" "${URL}"
  mv "${ZIP}.tmp" "${ZIP}"
fi

if ! verify_zip; then
  echo "[FURY][FAIL] AC Data ${VERSION} SHA-256 mismatch" >&2
  exit 1
fi

echo "[FURY][PASS] AC Data ${VERSION} archive checksum verified"

unzip -q "${ZIP}" -d "${STAGE}"

dbc_dir="$(find "${STAGE}" -maxdepth 3 -type d -name dbc -print -quit)"
if [[ -z "${dbc_dir}" ]]; then
  echo "[FURY][FAIL] extracted archive contains no dbc directory" >&2
  exit 1
fi

SOURCE_DIR="$(dirname "${dbc_dir}")"
for required in dbc maps vmaps; do
  if [[ ! -d "${SOURCE_DIR}/${required}" ]]; then
    echo "[FURY][FAIL] AC Data archive missing required directory: ${required}" >&2
    exit 1
  fi
done

rm -rf "${DATA_DIR}"
mkdir -p "${DATA_DIR}"

for dir in dbc maps vmaps mmaps cameras Cameras; do
  if [[ -d "${SOURCE_DIR}/${dir}" ]]; then
    mv "${SOURCE_DIR}/${dir}" "${DATA_DIR}/"
  fi
done

for required in dbc maps vmaps; do
  if ! find "${DATA_DIR}/${required}" -type f -print -quit | grep -q .; then
    echo "[FURY][FAIL] installed AC Data directory is empty: ${required}" >&2
    exit 1
  fi
done

echo "[FURY][PASS] AC Data ${VERSION} installed at ${DATA_DIR}"
du -sh "${DATA_DIR}" || true
