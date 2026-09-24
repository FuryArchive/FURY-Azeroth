#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="${ROOT}/vendor/lock/fury.lock.yaml"
CORE="${ROOT}/upstream/azerothcore-wotlk"
REPORT="${1:-${ROOT}/build/eluna-playerbots-merge-audit.txt}"

[[ -d "${CORE}/.git" ]] || { echo "[FURY][ELUNA][FAIL] Playerbots core workspace missing" >&2; exit 2; }

read_eluna() {
  python3 - "${LOCK}" "$1" <<'PY'
import json
import sys
with open(sys.argv[1], "r", encoding="utf-8") as fh:
    lock = json.load(fh)
node = lock["integrations"]["standard_eluna"]
print(node[sys.argv[2]])
PY
}

repository="$(read_eluna repository)"
commit="$(read_eluna commit)"

mkdir -p "$(dirname "${REPORT}")"
: > "${REPORT}"

echo "[FURY][ELUNA] Playerbots head: $(git -C "${CORE}" rev-parse HEAD)" | tee -a "${REPORT}"
echo "[FURY][ELUNA] Eluna source: ${repository}@${commit}" | tee -a "${REPORT}"

if git -C "${CORE}" remote get-url fury-eluna >/dev/null 2>&1; then
  git -C "${CORE}" remote set-url fury-eluna "https://github.com/${repository}.git"
else
  git -C "${CORE}" remote add fury-eluna "https://github.com/${repository}.git"
fi

git -C "${CORE}" fetch --no-tags fury-eluna "${commit}"
git -C "${CORE}" config user.name "FURY Integration Audit"
git -C "${CORE}" config user.email "fury-integration@invalid.local"

merge_base="$(git -C "${CORE}" merge-base HEAD "${commit}")"
echo "[FURY][ELUNA] merge-base: ${merge_base}" | tee -a "${REPORT}"

set +e
git -C "${CORE}" merge --no-commit --no-ff "${commit}" > /tmp/fury-eluna-merge.out 2>&1
merge_rc=$?
set -e

cat /tmp/fury-eluna-merge.out | tee -a "${REPORT}"

mapfile -t conflicts < <(git -C "${CORE}" diff --name-only --diff-filter=U)

{
  echo
  echo "merge_exit=${merge_rc}"
  echo "conflict_count=${#conflicts[@]}"
  if [[ "${#conflicts[@]}" -gt 0 ]]; then
    echo "conflicts:"
    printf '  %s\n' "${conflicts[@]}"
  fi
  echo
  echo "merged_change_summary:"
  git -C "${CORE}" diff --stat --cached || true
} | tee -a "${REPORT}"

git -C "${CORE}" merge --abort >/dev/null 2>&1 || git -C "${CORE}" reset --hard HEAD >/dev/null

if [[ "${#conflicts[@]}" -gt 0 ]]; then
  echo "[FURY][ELUNA][BLOCKED] three-way merge has ${#conflicts[@]} conflict(s)" | tee -a "${REPORT}"
  exit 1
fi

if [[ "${merge_rc}" -ne 0 ]]; then
  echo "[FURY][ELUNA][FAIL] merge failed without ordinary file conflicts" | tee -a "${REPORT}"
  exit "${merge_rc}"
fi

echo "[FURY][ELUNA][PASS] standard Eluna merges cleanly into pinned Playerbots core" | tee -a "${REPORT}"
