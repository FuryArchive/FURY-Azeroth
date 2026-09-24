#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="${1:-${ROOT}/upstream/azerothcore-wotlk}"
MODULES="${CORE}/modules"

fail() {
  echo "[FURY][AUTHORITY][FAIL] $*" >&2
  exit 1
}

pass() {
  echo "[FURY][AUTHORITY][PASS] $*"
}

require_line() {
  local file="$1"
  local pattern="$2"
  local message="$3"
  [[ -f "${file}" ]] || fail "missing authority input: ${file}"
  grep -Eq "${pattern}" "${file}" || fail "${message}"
  pass "${message}"
}

require_line \
  "${MODULES}/mod-zone-difficulty/conf/mod-zone-difficulty.conf.dist" \
  '^ModZoneDifficulty\.Mythicmode\.Enable[[:space:]]*=[[:space:]]*0[[:space:]]*$' \
  "Zone Difficulty built-in Mythic mode is disabled"

require_line \
  "${MODULES}/mod-autobalance/conf/AutoBalance.conf.dist" \
  '^AutoBalance\.reward\.enable[[:space:]]*=[[:space:]]*0[[:space:]]*$' \
  "AutoBalance experimental boss-token rewards are disabled"

require_line \
  "${MODULES}/mod-dungeon-master/conf/mod_dungeon_master.conf.dist" \
  '^DungeonMaster\.Rewards\.DirectPersistentEnabled[[:space:]]*=[[:space:]]*0[[:space:]]*$' \
  "Dungeon Master direct persistent rewards are FURY-gated"

require_line \
  "${MODULES}/mod-nemesis-system/conf/mod_nemesis_system.conf.dist" \
  '^NemesisSystem\.DirectPersistentRewards\.Enable[[:space:]]*=[[:space:]]*0[[:space:]]*$' \
  "Nemesis direct bounty/revenge rewards are FURY-gated"

progression_conf="${MODULES}/mod-progression-system/conf/progression_system.conf.dist"
[[ -f "${progression_conf}" ]] || fail "missing authority input: ${progression_conf}"
if grep -Eq '^ProgressionSystem\.Bracket_[^=]+=[[:space:]]*[1-9][0-9]*[[:space:]]*$' "${progression_conf}"; then
  fail "Progression System ships with an active bracket; FURY must choose the campaign bracket explicitly"
fi
pass "Progression System has no autonomous active bracket"

challenge_conf="${MODULES}/mod-challenge-modes/conf/challenge_modes.conf.dist"
[[ -f "${challenge_conf}" ]] || fail "missing authority input: ${challenge_conf}"
if grep -Eq '^(Hardcore|SemiHardcore|SelfCrafted|ItemQualityLevel|SlowXpGain|VerySlowXpGain|QuestXpOnly|IronMan)\.(TitleRewards|TalentRewards|ItemRewards|AchievementReward)[[:space:]]*=[[:space:]]*"[^"]+"' "${challenge_conf}"; then
  fail "Challenge Modes ships with direct durable rewards enabled"
fi
pass "Challenge Modes direct title/talent/item/achievement reward lists are empty"

echo "[FURY][AUTHORITY][PASS] selected server module authority gate passed"
