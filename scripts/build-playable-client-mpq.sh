#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROFILE="${1:-${ROOT}/build/playable-profile}"
RAW="${PROFILE}/client/mpq-root"
MPQ_NAME="${FURY_MPQ_NAME:-patch-Z.MPQ}"
OUT="${2:-${PROFILE}/client/Data/${MPQ_NAME}}"

MPQCLI_VERSION="v0.11.0"
MPQCLI_SHA256="a2583a938814b7dd32b116daf96376648b476e0399856c85f7013d31938ab9db"
MPQCLI_URL="https://github.com/thegraydot/mpqcli/releases/download/${MPQCLI_VERSION}/mpqcli-linux-amd64-glibc"
CACHE="${FURY_TOOL_CACHE:-${ROOT}/.cache/tools}"
PINNED="${CACHE}/mpqcli-${MPQCLI_VERSION}-linux-amd64-glibc"

fail() { echo "[FURY][MPQ][FAIL] $*" >&2; exit 1; }

[[ -d "${RAW}" ]] || fail "staged MPQ root missing: ${RAW}"

resolve_mpqcli() {
  if command -v mpqcli >/dev/null 2>&1; then
    command -v mpqcli
    return
  fi

  if [[ "$(uname -s)" != "Linux" || "$(uname -m)" != "x86_64" ]]; then
    fail "mpqcli not installed and pinned auto-download only supports Linux x86_64"
  fi

  mkdir -p "${CACHE}"
  if [[ ! -f "${PINNED}" ]] || [[ "$(sha256sum "${PINNED}" | awk '{print $1}')" != "${MPQCLI_SHA256}" ]]; then
    rm -f "${PINNED}" "${PINNED}.tmp"
    curl --fail --location --retry 4 --output "${PINNED}.tmp" "${MPQCLI_URL}"
    echo "${MPQCLI_SHA256}  ${PINNED}.tmp" | sha256sum -c - >/dev/null
    mv "${PINNED}.tmp" "${PINNED}"
    chmod +x "${PINNED}"
  fi

  echo "${PINNED}"
}

MPQCLI="$(resolve_mpqcli)"
rm -f "${OUT}"
mkdir -p "$(dirname "${OUT}")"

"${MPQCLI}" create --game wow-wotlk --output "${OUT}" "${RAW}"
[[ -s "${OUT}" ]] || fail "MPQ was not created"

list_file="$(mktemp)"
trap 'rm -f "${list_file}"' EXIT
"${MPQCLI}" list "${OUT}" > "${list_file}"

require_entry() {
  local pattern="$1"
  grep -Fqi "${pattern}" "${list_file}" || fail "archive entry missing: ${pattern}"
}

require_entry 'DBFilesClient\ChrRaces.dbc'
require_entry 'DBFilesClient\Map.dbc'
require_entry 'DBFilesClient\Item.dbc'
require_entry 'Interface\MythicPlus'
require_entry 'world\maps'

sha256sum "${OUT}" > "${OUT}.sha256"
echo "[FURY][MPQ][PASS] created ${OUT}"
cat "${OUT}.sha256"
