#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BRACKET="${1:-0}"
MIN_BOTS="${FURY_RANDOM_BOTS_MIN:-80}"
MAX_BOTS="${FURY_RANDOM_BOTS_MAX:-120}"

fail() { echo "[FURY][MODULES][FAIL] $*" >&2; exit 1; }

[[ "${MIN_BOTS}" =~ ^[0-9]+$ ]] || fail "FURY_RANDOM_BOTS_MIN must be an integer"
[[ "${MAX_BOTS}" =~ ^[0-9]+$ ]] || fail "FURY_RANDOM_BOTS_MAX must be an integer"
(( MIN_BOTS <= MAX_BOTS )) || fail "FURY_RANDOM_BOTS_MIN cannot exceed FURY_RANDOM_BOTS_MAX"

PLAYERBOTS="${ROOT}/etc/modules/playerbots.conf"
LIVING="${ROOT}/etc/modules/mod_living_world.conf"
ZONE="${ROOT}/etc/modules/mod-zone-difficulty.conf"
AHBOT="${ROOT}/etc/modules/mod_ahbot.conf"
PROGRESSION="${ROOT}/etc/modules/progression_system.conf"

for file in "${PLAYERBOTS}" "${LIVING}" "${ZONE}" "${AHBOT}" "${PROGRESSION}"; do
  [[ -f "${file}" ]] || fail "required playable module config missing: ${file}"
done

python3 - "${PLAYERBOTS}" "${LIVING}" "${ZONE}" "${AHBOT}" "${PROGRESSION}" "${BRACKET}" "${MIN_BOTS}" "${MAX_BOTS}" <<'PY'
from pathlib import Path
import re
import sys

playerbots, living, zone, ahbot, progression, bracket, min_bots, max_bots = sys.argv[1:9]
playerbots = Path(playerbots)
living = Path(living)
zone = Path(zone)
ahbot = Path(ahbot)
progression = Path(progression)

def set_option(path: Path, key: str, value: str) -> None:
    text = path.read_text()
    line = f"{key} = {value}"
    pattern = re.compile(rf"(?m)^\s*{re.escape(key)}\s*=.*$")
    if pattern.search(text):
        text = pattern.sub(lambda _: line, text, count=1)
    else:
        text = text.rstrip() + "\n" + line + "\n"
    path.write_text(text)

progression_text = progression.read_text()
order = [
    match.group(1)
    for match in re.finditer(
        r"(?m)^ProgressionSystem\.Bracket_([A-Za-z0-9_]+)\s*=\s*[01]\s*$",
        progression_text,
    )
    if match.group(1) != "Custom"
]
if bracket not in order:
    raise SystemExit(f"[FURY][MODULES][FAIL] unknown progression bracket: {bracket}")

wotlk_start = order.index("71_74")
wotlk_active = order.index(bracket) >= wotlk_start

# Private two-player realm: keep enough ambient population to feel alive without
# the 500-online upstream default, and do not burn CPU while nobody is playing.
set_option(playerbots, "AiPlayerbot.MinRandomBots", min_bots)
set_option(playerbots, "AiPlayerbot.MaxRandomBots", max_bots)
set_option(playerbots, "AiPlayerbot.DisabledWithoutRealPlayer", "1")
set_option(playerbots, "AiPlayerbot.SyncLevelWithPlayers", "1")
set_option(playerbots, "AiPlayerbot.RandomBotConcentrateInPlayerZone", "1")
set_option(playerbots, "AiPlayerbot.LimitTalentsExpansion", "1")
set_option(playerbots, "AiPlayerbot.DisableDeathKnightLogin", "0" if wotlk_active else "1")

# Living World ships with development debug enabled. Keep gameplay systems on,
# but use production logging. Its Playerbots integration is upstream-unimplemented
# at the pinned revision and deliberately remains off.
set_option(living, "LivingWorld.Enable", "1")
set_option(living, "LivingWorld.Playerbots.Enable", "0")
set_option(living, "LivingWorld.Debug", "0")
set_option(living, "LivingWorld.Invasions.Enable", "1")
set_option(living, "LivingWorld.Travelers.Enable", "1")

# Mythic+ Extended is the canonical Mythic owner in FURY.
set_option(zone, "ModZoneDifficulty.Enable", "1")
set_option(zone, "ModZoneDifficulty.Mythicmode.Enable", "0")
set_option(zone, "ModZoneDifficulty.MythicmodeAI.Enable", "0")

# AHBot needs a dedicated account/character. Keep it explicitly off until the
# safe configure-ahbot.sh step has selected one.
set_option(ahbot, "AuctionHouseBot.EnableSeller", "0")
set_option(ahbot, "AuctionHouseBot.EnableBuyer", "0")

print(
    f"[FURY][MODULES][PASS] bracket={bracket} "
    f"randombots={min_bots}-{max_bots} "
    f"death_knights={'on' if wotlk_active else 'off'}"
)
PY
