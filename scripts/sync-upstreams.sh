#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="${ROOT}/vendor/lock/fury.lock.yaml"
UPSTREAM="${ROOT}/upstream"
CORE_DIR="${UPSTREAM}/azerothcore-wotlk"
PROFILE="${FURY_STACK_PROFILE:-baseline}"

case "${PROFILE}" in
  baseline|server|all) ;;
  *)
    echo "[FURY] unknown stack profile '${PROFILE}' (expected baseline|server|all)" >&2
    exit 1
    ;;
esac

if [[ ! -f "${LOCK}" ]]; then
  echo "[FURY] missing lock file: ${LOCK}" >&2
  exit 1
fi

read_lock() {
  local path="$1"
  local field="$2"

  python3 - "${LOCK}" "${path}" "${field}" <<'PY'
import json
import sys

lock_path, dotted_path, field = sys.argv[1:4]
with open(lock_path, "r", encoding="utf-8") as fh:
    data = json.load(fh)

node = data
for part in dotted_path.split("."):
    node = node[part]

value = node[field]
if not isinstance(value, (str, int, float, bool)):
    raise SystemExit(f"lock value {dotted_path}.{field} is not scalar")
print(value)
PY
}

clone_pin() {
  local key="$1"
  local dest="$2"

  local repository branch commit
  repository="$(read_lock "${key}" repository)"
  branch="$(read_lock "${key}" branch)"
  commit="$(read_lock "${key}" commit)"

  if [[ ! -d "${dest}/.git" ]]; then
    echo "[FURY] clone ${repository} -> ${dest}"
    mkdir -p "$(dirname "${dest}")"
    git clone --filter=blob:none --no-checkout "https://github.com/${repository}.git" "${dest}"
  fi

  echo "[FURY] sync ${repository} @ ${commit}"
  git -C "${dest}" remote set-url origin "https://github.com/${repository}.git"
  git -C "${dest}" fetch --prune origin "${branch}"
  git -C "${dest}" fetch origin "${commit}"
  git -C "${dest}" checkout --detach "${commit}"
  git -C "${dest}" reset --hard "${commit}"

  if [[ "${key}" == "modules.living_world" ]]; then
    git -C "${dest}" clean -f -- src/invasions/RuntimeCompletionObserver.h
  fi

  local actual
  actual="$(git -C "${dest}" rev-parse HEAD)"
  if [[ "${actual}" != "${commit}" ]]; then
    echo "[FURY] pin mismatch for ${repository}: expected ${commit}, got ${actual}" >&2
    exit 1
  fi
}

apply_patches() {
  local key="$1"
  local dest="$2"
  local field="${3:-patches}"

  mapfile -t patches < <(python3 - "${LOCK}" "${key}" "${field}" <<'PY'
import json
import sys

lock_path, dotted_path, field = sys.argv[1:4]
with open(lock_path, "r", encoding="utf-8") as fh:
    data = json.load(fh)

node = data
for part in dotted_path.split("."):
    node = node[part]

for patch in node.get(field, []):
    print(patch)
PY
)

  for relative_patch in "${patches[@]}"; do
    local patch="${ROOT}/${relative_patch}"
    if [[ ! -f "${patch}" ]]; then
      echo "[FURY] missing upstream patch: ${relative_patch}" >&2
      exit 1
    fi

    echo "[FURY] verify patch ${relative_patch}"
    if ! git -C "${dest}" apply --check "${patch}"; then
      echo "[FURY] compatibility guard failed: ${relative_patch} no longer applies cleanly to the pinned upstream." >&2
      exit 1
    fi

    git -C "${dest}" apply "${patch}"
  done
}

list_module_keys() {
  python3 - "${LOCK}" "${PROFILE}" <<'PY'
import json
import sys

lock_path, profile = sys.argv[1:3]
with open(lock_path, "r", encoding="utf-8") as fh:
    data = json.load(fh)

for key, module in data["modules"].items():
    tier = module.get("tier", "baseline")
    if tier == "baseline" or profile in ("server", "all"):
        print(f"{key}\t{module['directory']}")
PY
}

list_integration_keys() {
  python3 - "${LOCK}" "${PROFILE}" <<'PY'
import json
import sys

lock_path, profile = sys.argv[1:3]
if profile != "all":
    raise SystemExit(0)

with open(lock_path, "r", encoding="utf-8") as fh:
    data = json.load(fh)

for key, integration in data.get("integrations", {}).items():
    if integration.get("selected", False):
        print(f"{key}\t{integration['directory']}\t{integration.get('core_module_directory', '')}")
PY
}

mkdir -p "${UPSTREAM}"

clone_pin "core" "${CORE_DIR}"
mkdir -p "${CORE_DIR}/modules"

while IFS=$'\t' read -r key directory; do
  [[ -n "${key}" ]] || continue
  dest="${CORE_DIR}/modules/${directory}"
  clone_pin "modules.${key}" "${dest}"
  apply_patches "modules.${key}" "${dest}"
done < <(list_module_keys)

if [[ -d "${ROOT}/modules/mod-fury" ]]; then
  rm -rf "${CORE_DIR}/modules/mod-fury"
  ln -s "${ROOT}/modules/mod-fury" "${CORE_DIR}/modules/mod-fury"
  echo "[FURY] linked first-party module: modules/mod-fury"
fi

if [[ "${PROFILE}" == "all" ]]; then
  INTEGRATIONS_DIR="${UPSTREAM}/integrations"
  mkdir -p "${INTEGRATIONS_DIR}"
  while IFS=

echo
echo "[FURY] resolved workspace profile: ${PROFILE}"
printf "  core: %s\n" "$(git -C "${CORE_DIR}" rev-parse HEAD)"

while IFS=$'\t' read -r key directory; do
  [[ -n "${key}" ]] || continue
  printf "  %-24s %s\n" "${key}:" "$(git -C "${CORE_DIR}/modules/${directory}" rev-parse HEAD)"
done < <(list_module_keys)

if [[ "${PROFILE}" == "all" ]]; then
  echo "  integrations:"
  while IFS=
fi

echo
echo "[FURY] upstream sync complete."
\t' read -r key directory core_module_directory; do
    [[ -n "${key}" ]] || continue
    dest="${INTEGRATIONS_DIR}/${directory}"
    clone_pin "integrations.${key}" "${dest}"
    apply_patches "integrations.${key}" "${dest}"

    if [[ -n "${core_module_directory}" ]]; then
      rm -rf "${CORE_DIR}/modules/${core_module_directory}"
      ln -s "${dest}" "${CORE_DIR}/modules/${core_module_directory}"
      echo "[FURY] linked integration module: modules/${core_module_directory}"
      apply_patches "integrations.${key}" "${CORE_DIR}" "core_patches"
    fi
  done < <(list_integration_keys)
fi

echo
echo "[FURY] resolved workspace profile: ${PROFILE}"
printf "  core: %s\n" "$(git -C "${CORE_DIR}" rev-parse HEAD)"

while IFS=$'\t' read -r key directory; do
  [[ -n "${key}" ]] || continue
  printf "  %-24s %s\n" "${key}:" "$(git -C "${CORE_DIR}/modules/${directory}" rev-parse HEAD)"
done < <(list_module_keys)

if [[ "${PROFILE}" == "all" ]]; then
  echo "  integrations:"
  while IFS=$'\t' read -r key directory; do
    [[ -n "${key}" ]] || continue
    printf "    %-22s %s\n" "${key}:" "$(git -C "${UPSTREAM}/integrations/${directory}" rev-parse HEAD)"
  done < <(list_integration_keys)
fi

echo
echo "[FURY] upstream sync complete."
\t' read -r key directory core_module_directory; do
    [[ -n "${key}" ]] || continue
    printf "    %-22s %s\n" "${key}:" "$(git -C "${UPSTREAM}/integrations/${directory}" rev-parse HEAD)"
  done < <(list_integration_keys)
fi

echo
echo "[FURY] upstream sync complete."
\t' read -r key directory core_module_directory; do
    [[ -n "${key}" ]] || continue
    dest="${INTEGRATIONS_DIR}/${directory}"
    clone_pin "integrations.${key}" "${dest}"
    apply_patches "integrations.${key}" "${dest}"

    if [[ -n "${core_module_directory}" ]]; then
      rm -rf "${CORE_DIR}/modules/${core_module_directory}"
      ln -s "${dest}" "${CORE_DIR}/modules/${core_module_directory}"
      echo "[FURY] linked integration module: modules/${core_module_directory}"
      apply_patches "integrations.${key}" "${CORE_DIR}" "core_patches"
    fi
  done < <(list_integration_keys)
fi

echo
echo "[FURY] resolved workspace profile: ${PROFILE}"
printf "  core: %s\n" "$(git -C "${CORE_DIR}" rev-parse HEAD)"

while IFS=$'\t' read -r key directory; do
  [[ -n "${key}" ]] || continue
  printf "  %-24s %s\n" "${key}:" "$(git -C "${CORE_DIR}/modules/${directory}" rev-parse HEAD)"
done < <(list_module_keys)

if [[ "${PROFILE}" == "all" ]]; then
  echo "  integrations:"
  while IFS=$'\t' read -r key directory; do
    [[ -n "${key}" ]] || continue
    printf "    %-22s %s\n" "${key}:" "$(git -C "${UPSTREAM}/integrations/${directory}" rev-parse HEAD)"
  done < <(list_integration_keys)
fi

echo
echo "[FURY] upstream sync complete."
