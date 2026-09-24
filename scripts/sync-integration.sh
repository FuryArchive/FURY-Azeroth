#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="${ROOT}/vendor/lock/fury.lock.yaml"
UPSTREAM="${ROOT}/upstream/integrations"
KEY="${1:-}"

if [[ -z "${KEY}" ]]; then
  echo "usage: $0 <integration-key>" >&2
  exit 2
fi

read_field() {
  python3 - "${LOCK}" "${KEY}" "$1" <<'PY'
import json
import sys
lock_path, key, field = sys.argv[1:4]
with open(lock_path, "r", encoding="utf-8") as fh:
    data = json.load(fh)
node = data["integrations"][key]
value = node[field]
if isinstance(value, bool):
    print("true" if value else "false")
else:
    print(value)
PY
}

repository="$(read_field repository)"
branch="$(read_field branch)"
commit="$(read_field commit)"
directory="$(read_field directory)"
dest="${UPSTREAM}/${directory}"

mkdir -p "${UPSTREAM}"
if [[ ! -d "${dest}/.git" ]]; then
  echo "[FURY] clone integration ${repository} -> ${dest}"
  git clone --filter=blob:none --no-checkout "https://github.com/${repository}.git" "${dest}"
fi

git -C "${dest}" remote set-url origin "https://github.com/${repository}.git"
git -C "${dest}" fetch --prune origin "${branch}"
git -C "${dest}" fetch origin "${commit}"
git -C "${dest}" checkout --detach "${commit}"
git -C "${dest}" reset --hard "${commit}"

actual="$(git -C "${dest}" rev-parse HEAD)"
[[ "${actual}" == "${commit}" ]] || { echo "[FURY][FAIL] integration pin mismatch: ${actual}" >&2; exit 1; }

echo "[FURY][PASS] integration ${KEY}: ${repository}@${actual}"
echo "${dest}"
