#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="${ROOT}/vendor/lock/fury.lock.yaml"
CORE="${ROOT}/upstream/azerothcore-wotlk"

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

if git -C "${CORE}" remote get-url fury-eluna >/dev/null 2>&1; then
  git -C "${CORE}" remote set-url fury-eluna "https://github.com/${repository}.git"
else
  git -C "${CORE}" remote add fury-eluna "https://github.com/${repository}.git"
fi

git -C "${CORE}" fetch --no-tags fury-eluna "${commit}"
git -C "${CORE}" config user.name "FURY Integration"
git -C "${CORE}" config user.email "fury-integration@invalid.local"

# Integration profiles may apply narrow tracked core patches (for example
# playable Goblin/Worgen support) before Eluna is reconciled. Commit those
# generated-workspace changes first so the three-way merge has a clean index.
if [[ -n "$(git -C "${CORE}" status --porcelain --untracked-files=no)" ]]; then
  echo "[FURY][ELUNA] commit pre-Eluna FURY core patches"
  git -C "${CORE}" add -u
  git -C "${CORE}" commit -m "FURY generated pre-Eluna core patches"
fi

echo "[FURY][ELUNA] merge standard Eluna ${commit} into Playerbots $(git -C "${CORE}" rev-parse HEAD)"

set +e
git -C "${CORE}" merge --no-commit --no-ff "${commit}"
merge_rc=$?
set -e

mapfile -t conflicts < <(git -C "${CORE}" diff --name-only --diff-filter=U)

allowed_runtime=(
  "src/server/apps/worldserver/worldserver.conf.dist"
  "src/server/game/Entities/Object/Object.cpp"
  "src/server/game/Entities/Object/Object.h"
)

is_allowed_runtime() {
  local candidate="$1"
  local allowed
  for allowed in "${allowed_runtime[@]}"; do
    [[ "${candidate}" == "${allowed}" ]] && return 0
  done
  return 1
}

for path in "${conflicts[@]}"; do
  if [[ "${path}" == .github/workflows/* ]]; then
    echo "[FURY][ELUNA] keep Playerbots workflow: ${path}"
    git -C "${CORE}" checkout --ours -- "${path}"
    git -C "${CORE}" add "${path}"
  elif is_allowed_runtime "${path}"; then
    echo "[FURY][ELUNA] reconcile runtime file: ${path}"
    git -C "${CORE}" checkout --ours -- "${path}"
  else
    echo "[FURY][ELUNA][FAIL] unexpected merge conflict: ${path}" >&2
    git -C "${CORE}" merge --abort >/dev/null 2>&1 || true
    exit 1
  fi
done

python3 - "${CORE}" <<'PY'
from pathlib import Path
import sys

core = Path(sys.argv[1])

def replace_once(text, old, new, label):
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"[FURY][ELUNA][FAIL] merge marker not found for {label}")
    return text.replace(old, new, 1)

# Object.h: preserve Playerbots' WorldObject extensions and add Eluna state beside them.
path = core / "src/server/game/Entities/Object/Object.h"
text = path.read_text(encoding="utf-8")
text = replace_once(
    text,
    '#include <string>\n\n#include "UpdateFields.h"',
    '#include <string>\n#ifdef ELUNA\n#include "ElunaEventMgr.h"\n#include "LuaValue.h"\n#endif\n\n#include "UpdateFields.h"',
    "Object.h Eluna includes",
)
text = replace_once(
    text,
    'class ALEEventProcessor;\n\nenum TempSummonType',
    'class ALEEventProcessor;\n#ifdef ELUNA\nclass ElunaEventProcessorInfo;\nclass ElunaEventProcessor;\nclass Eluna;\n#endif\n\nenum TempSummonType',
    "Object.h Eluna forward declarations",
)
text = replace_once(
    text,
    '    ALEEventProcessor* ALEEvents;\n    EventProcessor m_Events;\n\n    // CastSpell',
    '    ALEEventProcessor* ALEEvents;\n    EventProcessor m_Events;\n\n#ifdef ELUNA\n    std::unique_ptr<ElunaProcessorInfo> elunaMapEvents;\n    std::unique_ptr<ElunaProcessorInfo> elunaWorldEvents;\n\n    Eluna* GetEluna() const;\n    ElunaEventProcessor* GetElunaEvents(int32 mapId);\n\n    LuaVal lua_data = LuaVal({});\n#endif\n\n    // CastSpell',
    "Object.h Eluna members",
)
path.write_text(text, encoding="utf-8")

# Object.cpp: Eluna include, map-processor reset and WorldObject event accessors.
path = core / "src/server/game/Entities/Object/Object.cpp"
text = path.read_text(encoding="utf-8")
text = replace_once(
    text,
    '#include "Totem.h"\n#include "Transport.h"',
    '#include "Totem.h"\n#ifdef ELUNA\n#include "LuaEngine.h"\n#endif\n#include "Transport.h"',
    "Object.cpp Eluna include",
)
text = replace_once(
    text,
    '    m_currMap = map;\n    m_mapId = map->GetId();\n    m_InstanceId = map->GetInstanceId();\n\n    sScriptMgr->OnWorldObjectSetMap(this, map);',
    '    m_currMap = map;\n    m_mapId = map->GetId();\n    m_InstanceId = map->GetInstanceId();\n#ifdef ELUNA\n    if (elunaMapEvents)\n        elunaMapEvents.reset();\n#endif\n\n    sScriptMgr->OnWorldObjectSetMap(this, map);',
    "Object.cpp SetMap",
)
eluna_tail = r'''
#ifdef ELUNA
Eluna* WorldObject::GetEluna() const
{
    if (const Map* map = FindMap())
        return map->GetEluna();

    return nullptr;
}

ElunaEventProcessor* WorldObject::GetElunaEvents(int32 mapId)
{
    Eluna* eluna = mapId == -1 ? sWorld->GetEluna() : GetEluna();
    if (!eluna)
        return nullptr;

    EventMgr* mgr = eluna->eventMgr.get();
    if (!mgr)
        return nullptr;

    std::unique_ptr<ElunaProcessorInfo>& info = (mapId == -1) ? elunaWorldEvents : elunaMapEvents;

    if (!info)
    {
        uint64 id = mgr->CreateObjectProcessor(this);
        info = std::make_unique<ElunaProcessorInfo>(mgr, id);
    }

    return mgr->GetObjectProcessor(info->GetProcessorId());
}
#endif
'''
if 'Eluna* WorldObject::GetEluna() const' not in text:
    text = text.rstrip() + "\n\n" + eluna_tail.lstrip()
path.write_text(text, encoding="utf-8")

# worldserver.conf.dist: keep Playerbots logging/settings and add Eluna logging/settings.
path = core / "src/server/apps/worldserver/worldserver.conf.dist"
text = path.read_text(encoding="utf-8")
text = replace_once(
    text,
    'Appender.Playerbots=2,5,0,Playerbots.log,w',
    'Appender.Playerbots=2,5,0,Playerbots.log,w\nAppender.Eluna=2,5,3,Eluna.log,a',
    "worldserver Eluna appender",
)
text = replace_once(
    text,
    'Logger.playerbots=5,Console Playerbots',
    'Logger.playerbots=5,Console Playerbots\nLogger.eluna=1,Console Eluna',
    "worldserver Eluna logger",
)

settings = r'''
###################################################################################################
#  ELUNA SETTINGS
#
#   Eluna.Enabled
#       Description: Enable or disable Eluna LuaEngine.
#       Default:     true  - (enabled)
#                    false - (disabled)
#
#   Eluna.TraceBack
#       Description: Sets whether to use debug.traceback function on a lua error or not.
#       Default:     false
#
#   Eluna.ScriptReloader
#       Description: Automatically reload scripts when changes are detected.
#       Default:     false
#
#   Eluna.ReloadCommand
#       Description: Enable or disable the .reload eluna command.
#       Default:     true
#
#   Eluna.UseUnsafeMethods
#       Description: Allow standard-Eluna unsafe API methods.
#       Default:     true
#
#   Eluna.UseDeprecatedMethods
#       Description: Allow standard-Eluna deprecated API methods.
#       Default:     true
#
#   Eluna.OnlyOnMaps
#       Description: Optional map allowlist for multistate mode.
#       Default:     ""
#
#   Eluna.ScriptPath
#       Description: Lua script directory.
#       Default:     "lua_scripts"
#
#   Eluna.RequirePaths / Eluna.RequireCPaths
#       Description: Additional Lua/Lua-C require search paths.
#       Default:     ""
#
#   Eluna.ReloadSecurityLevel
#       Description: Security level required for .reload eluna.
#       Default:     3
#
Eluna.Enabled = true
Eluna.TraceBack = false
Eluna.ScriptReloader = false
Eluna.ReloadCommand = true
Eluna.UseUnsafeMethods = true
Eluna.UseDeprecatedMethods = true
Eluna.OnlyOnMaps = ""
Eluna.ScriptPath = "lua_scripts"
Eluna.RequirePaths = ""
Eluna.RequireCPaths = ""
Eluna.ReloadSecurityLevel = 3

#
###################################################################################################

'''
if 'Eluna.Enabled = true' not in text:
    marker = '###################################################################################################\n#                                                                                                 #\n# GAME SETTINGS END'
    if marker not in text:
        raise SystemExit("[FURY][ELUNA][FAIL] GAME SETTINGS END marker missing")
    text = text.replace(marker, settings + marker, 1)
path.write_text(text, encoding="utf-8")
PY

git -C "${CORE}" add   src/server/apps/worldserver/worldserver.conf.dist   src/server/game/Entities/Object/Object.cpp   src/server/game/Entities/Object/Object.h

mapfile -t unresolved < <(git -C "${CORE}" diff --name-only --diff-filter=U)
if [[ "${#unresolved[@]}" -ne 0 ]]; then
  printf '[FURY][ELUNA][FAIL] unresolved conflicts:\n' >&2
  printf '  - %s\n' "${unresolved[@]}" >&2
  git -C "${CORE}" merge --abort >/dev/null 2>&1 || true
  exit 1
fi

if [[ "${merge_rc}" -ne 0 && "${#conflicts[@]}" -eq 0 ]]; then
  echo "[FURY][ELUNA][FAIL] merge failed without resolvable file conflicts" >&2
  git -C "${CORE}" merge --abort >/dev/null 2>&1 || true
  exit "${merge_rc}"
fi

git -C "${CORE}" commit -m "FURY generated Playerbots + standard Eluna reconciliation"

git -C "${CORE}" submodule sync --recursive
git -C "${CORE}" submodule update --init --recursive

echo "[FURY][ELUNA][PASS] reconciled core: $(git -C "${CORE}" rev-parse HEAD)"
echo "[FURY][ELUNA][PASS] LuaEngine: $(git -C "${CORE}/src/server/game/LuaEngine" rev-parse HEAD)"
