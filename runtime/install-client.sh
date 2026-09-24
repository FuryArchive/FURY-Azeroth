#!/usr/bin/env bash
set -euo pipefail

SOURCE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${1:-}"
REALM="${2:-127.0.0.1}"

[[ -n "${TARGET}" ]] || { echo "usage: $0 /path/to/WoW-3.3.5a [realm-address]" >&2; exit 2; }
[[ -d "${TARGET}/Data" ]] || { echo "[FURY][CLIENT][FAIL] WoW Data directory not found: ${TARGET}/Data" >&2; exit 1; }

mkdir -p "${TARGET}/Interface/AddOns"
if [[ -f "${TARGET}/Data/patch-Z.MPQ" ]]; then
  backup="${TARGET}/Data/patch-Z.MPQ.fury-backup-$(date +%Y%m%d-%H%M%S)"
  mv "${TARGET}/Data/patch-Z.MPQ" "${backup}"
  echo "[FURY] backed up existing patch-Z.MPQ -> ${backup}"
fi

cp -a "${SOURCE}/Data/." "${TARGET}/Data/"
cp -a "${SOURCE}/Interface/AddOns/." "${TARGET}/Interface/AddOns/"

found=0
while IFS= read -r -d '' file; do
  printf 'set realmlist %s\n' "${REALM}" > "${file}"
  echo "[FURY] realmlist -> ${file}"
  found=1
done < <(find "${TARGET}/Data" -mindepth 2 -maxdepth 2 -type f -iname 'realmlist.wtf' -print0)

if [[ "${found}" -eq 0 ]]; then
  locale_found=0
  for locale in enUS enGB deDE esES esMX frFR ruRU koKR zhCN zhTW; do
    locale_dir="${TARGET}/Data/${locale}"
    [[ -d "${locale_dir}" ]] || continue
    printf 'set realmlist %s\n' "${REALM}" > "${locale_dir}/realmlist.wtf"
    echo "[FURY] created Data/${locale}/realmlist.wtf"
    locale_found=1
  done

  if [[ "${locale_found}" -eq 0 ]]; then
    mkdir -p "${TARGET}/Data/enUS"
    printf 'set realmlist %s\n' "${REALM}" > "${TARGET}/Data/enUS/realmlist.wtf"
    echo "[FURY] no locale directory detected; created Data/enUS/realmlist.wtf"
  fi
fi

echo "[FURY][CLIENT][PASS] FURY overlay installed into ${TARGET}"
