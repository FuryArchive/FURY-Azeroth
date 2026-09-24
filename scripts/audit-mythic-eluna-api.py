#!/usr/bin/env python3
from pathlib import Path
import argparse,re

ap=argparse.ArgumentParser()
ap.add_argument("--lua-dir",type=Path,required=True)
ap.add_argument("--eluna-dir",type=Path,required=True)
args=ap.parse_args()

files=sorted(args.lua_dir.glob("*.lua"))
source="\n".join(p.read_text(encoding="utf-8",errors="ignore") for p in args.eluna_dir.rglob("*") if p.is_file() and p.suffix in {".h",".hpp",".cpp"})
methods=set(); globals_=set()
for p in files:
    t=p.read_text(encoding="utf-8",errors="replace")
    methods.update(re.findall(r":\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(",t))
    globals_.update(re.findall(r"(?<![\w.:])((?:Register|Create|Remove|GetPlayerByGUID|SendMail)[A-Za-z_0-9]*)\s*\(",t))
# Client WoW API calls are in Mythic_Client.lua and are intentionally not Eluna API.
server_methods=set()
server_globals=set()
for p in files:
    if p.name=="Mythic_Client.lua": continue
    t=p.read_text(encoding="utf-8",errors="replace")
    server_methods.update(re.findall(r":\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(",t))
    server_globals.update(re.findall(r"(?<![\w.:])((?:Register|Create|Remove|GetPlayerByGUID|SendMail)[A-Za-z_0-9]*)\s*\(",t))

missing_m=[x for x in sorted(server_methods) if f'"{x}"' not in source]
missing_g=[x for x in sorted(server_globals) if x not in source]
if missing_m or missing_g:
    if missing_m: print("[FURY][MYTHIC][FAIL] missing Eluna methods:",", ".join(missing_m))
    if missing_g: print("[FURY][MYTHIC][FAIL] missing Eluna globals:",", ".join(missing_g))
    raise SystemExit(1)
print(f"[FURY][MYTHIC][PASS] standard Eluna API covers {len(server_methods)} methods and {len(server_globals)} globals")
