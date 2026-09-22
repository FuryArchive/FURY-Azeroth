#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="${ROOT}/vendor/lock/fury.lock.yaml"
TARGET="${FURY_SERVER_DATA_DIR:-${ROOT}/build/server-data}"
CACHE_DIR="${FURY_SERVER_DATA_CACHE_DIR:-${ROOT}/build/client-data-cache}"

read_client_data() {
  local field="$1"
  python3 - "${LOCK}" "${field}" <<'PY'
import json
import sys

lock_path, field = sys.argv[1:3]
with open(lock_path, "r", encoding="utf-8") as fh:
    data = json.load(fh)

value = data["client_data"][field]
if isinstance(value, list):
    print("\n".join(str(item) for item in value))
else:
    print(value)
PY
}

repository="$(read_client_data repository)"
release="$(read_client_data release)"
asset="$(read_client_data asset)"
sha256="$(read_client_data sha256)"
marker="${TARGET}/.fury-client-data.sha256"

if [[ -f "${marker}" ]] && [[ "$(cat "${marker}")" == "${sha256}" ]]; then
  missing=0
  for dir in dbc maps vmaps; do
    if [[ ! -d "${TARGET}/${dir}" ]]; then
      missing=1
      break
    fi
  done

  if [[ "${missing}" -eq 0 ]]; then
    echo "[FURY] pinned server data already present: ${release}"
    exit 0
  fi
fi

mkdir -p "${CACHE_DIR}"
archive="${CACHE_DIR}/${release}-${asset}"
url="https://github.com/${repository}/releases/download/${release}/${asset}"

if [[ ! -f "${archive}" ]] || ! echo "${sha256}  ${archive}" | sha256sum --check --status; then
  rm -f "${archive}.tmp"
  echo "[FURY] downloading pinned server data ${release}"
  curl --fail --location --retry 4 --retry-delay 5 --retry-all-errors \
    --output "${archive}.tmp" "${url}"
  mv "${archive}.tmp" "${archive}"
fi

echo "${sha256}  ${archive}" | sha256sum --check

rm -rf "${TARGET}"
mkdir -p "${TARGET}"

python3 - "${archive}" "${TARGET}" <<'PY'
from pathlib import Path, PurePosixPath
import shutil
import sys
import zipfile

archive = Path(sys.argv[1])
target = Path(sys.argv[2])
wanted_dirs = {"dbc", "maps", "vmaps", "cameras"}
copied = {name: 0 for name in wanted_dirs}

with zipfile.ZipFile(archive) as zf:
    for info in zf.infolist():
        if info.is_dir():
            continue

        parts = PurePosixPath(info.filename).parts
        lowered = [part.lower() for part in parts]

        root_index = None
        canonical_root = None
        for index, part in enumerate(lowered):
            if part in wanted_dirs:
                root_index = index
                canonical_root = parts[index]
                break

        if root_index is None:
            if parts and parts[-1].lower() == "data-version":
                destination = target / "data-version"
            else:
                continue
        else:
            relative_parts = parts[root_index:]
            destination = target.joinpath(*relative_parts)

        destination.parent.mkdir(parents=True, exist_ok=True)
        with zf.open(info) as source, destination.open("wb") as sink:
            shutil.copyfileobj(source, sink, length=1024 * 1024)

        if root_index is not None:
            copied[canonical_root.lower()] += 1

for required in ("dbc", "maps", "vmaps"):
    if copied[required] == 0:
        raise SystemExit(f"[FURY][FAIL] archive did not contain required {required}/ payload")

print(
    "[FURY] extracted startup data: "
    + ", ".join(f"{name}={copied[name]}" for name in ("dbc", "maps", "vmaps", "cameras"))
)
PY

printf '%s' "${sha256}" > "${marker}"

# The archive is large and the extracted subset is what Actions caches.
rm -f "${archive}"

echo "[FURY][PASS] pinned AzerothCore server data ready at ${TARGET}"
