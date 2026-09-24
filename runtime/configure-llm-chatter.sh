#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONF="${ROOT}/etc/modules/mod_llm_chatter.conf"
DB_PASSWORD="${FURY_MYSQL_PASSWORD:-fury}"
MODE="${FURY_LLM_ENABLE:-auto}"
EXPLICIT_MODEL="${FURY_LLM_MODEL:-}"

fail() { echo "[FURY][LLM][FAIL] $*" >&2; exit 1; }

[[ -f "${CONF}" ]] || {
  echo "[FURY][LLM] mod-llm-chatter config not present; skipping"
  exit 0
}

case "${MODE}" in
  auto|0|1) ;;
  *) fail "FURY_LLM_ENABLE must be auto, 0 or 1" ;;
esac

python3 - "${CONF}" "${MODE}" "${EXPLICIT_MODEL}" "${DB_PASSWORD}" <<'PY'
from pathlib import Path
import json
import re
import sys
import urllib.request

path = Path(sys.argv[1])
mode = sys.argv[2]
explicit_model = sys.argv[3].strip()
db_password = sys.argv[4]
text = path.read_text()

def set_option(payload: str, key: str, value: str) -> str:
    line = f"{key} = {value}"
    pattern = re.compile(rf"(?m)^\s*{re.escape(key)}\s*=.*$")
    if pattern.search(payload):
        return pattern.sub(lambda _: line, payload, count=1)
    return payload.rstrip() + "\n" + line + "\n"

def parameter_billions(model: dict) -> float | None:
    value = str(model.get("details", {}).get("parameter_size", "")).upper().strip()
    match = re.search(r"([0-9]+(?:\.[0-9]+)?)\s*B", value)
    return float(match.group(1)) if match else None

def detect_model() -> str:
    try:
        with urllib.request.urlopen("http://127.0.0.1:11434/api/tags", timeout=1.5) as response:
            payload = json.load(response)
    except Exception:
        return ""

    models = [
        model for model in payload.get("models", [])
        if model.get("name") and "embed" not in model["name"].lower()
    ]
    if not models:
        return ""

    preferred = []
    for model in models:
        params = parameter_billions(model)
        if params is not None and 3.5 <= params <= 8.5:
            preferred.append((params, int(model.get("size", 0)), model["name"]))
    if preferred:
        preferred.sort()
        return preferred[0][2]

    models.sort(key=lambda model: int(model.get("size", 0)) or 2**63)
    return models[0]["name"]

detected_model = explicit_model or detect_model()
if mode == "0":
    enabled = False
elif mode == "1":
    if not detected_model:
        raise SystemExit(
            "[FURY][LLM][FAIL] LLM chatter was forced on but no FURY_LLM_MODEL was supplied "
            "and no local Ollama model was detected"
        )
    enabled = True
else:
    enabled = bool(detected_model)

text = set_option(text, "LLMChatter.Enable", "1" if enabled else "0")
text = set_option(text, "LLMChatter.Provider", "ollama")
text = set_option(text, "LLMChatter.Model", detected_model or "qwen3:4b-instruct")
text = set_option(text, "LLMChatter.Ollama.BaseUrl", "http://host.docker.internal:11434")
text = set_option(text, "LLMChatter.Ollama.DisableThinking", "1")
text = set_option(text, "LLMChatter.Database.Host", "mysql")
text = set_option(text, "LLMChatter.Database.Port", "3306")
text = set_option(text, "LLMChatter.Database.User", "root")
text = set_option(text, "LLMChatter.Database.Password", db_password)
text = set_option(text, "LLMChatter.Database.Name", "acore_characters")
text = set_option(text, "LLMChatter.HealthCheck.Enable", "1")
text = set_option(text, "LLMChatter.HealthCheck.LLMProbe", "1" if enabled else "0")
path.write_text(text)

if enabled:
    print(f"[FURY][LLM][PASS] local Ollama chatter enabled with model: {detected_model}")
else:
    print("[FURY][LLM] no local Ollama model detected; chatter disabled without blocking the realm")
    print("[FURY][LLM] later: FURY_LLM_MODEL=<ollama-model> ./runtime/configure-llm-chatter.sh")
PY
